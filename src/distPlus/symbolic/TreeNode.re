/* This module represents a tree node. */
open SymbolicTypes;

type leaf = [
  | `SymbolicDist(SymbolicTypes.symbolicDist)
  | `RenderedDist(DistTypes.shape)
];

/* TreeNodes are either Data (i.e. symbolic or rendered distributions) or Operations. Operations always refer to two child nodes.*/
type treeNode = [ | `Leaf(leaf) | `Operation(operation)]
and operation = [
  | `AlgebraicCombination(algebraicOperation, treeNode, treeNode)
  | `PointwiseCombination(pointwiseOperation, treeNode, treeNode)
  | `VerticalScaling(scaleOperation, treeNode, treeNode)
  | `Render(treeNode)
  | `Truncate(option(float), option(float), treeNode)
  | `Normalize(treeNode)
  | `FloatFromDist(distToFloatOperation, treeNode)
];

module Operation = {
  type t = operation;
  let truncateToString =
      (left: option(float), right: option(float), nodeToString) => {
    let left = left |> E.O.dimap(Js.Float.toString, () => "-inf");
    let right = right |> E.O.dimap(Js.Float.toString, () => "inf");
    {j|truncate($nodeToString, $left, $right)|j};
  };

  let toString = nodeToString =>
    fun
    | `AlgebraicCombination(op, t1, t2) =>
      SymbolicTypes.Algebraic.format(op, nodeToString(t1), nodeToString(t2))
    | `PointwiseCombination(op, t1, t2) =>
      SymbolicTypes.Pointwise.format(op, nodeToString(t1), nodeToString(t2))
    | `VerticalScaling(scaleOp, t, scaleBy) =>
      SymbolicTypes.Scale.format(
        scaleOp,
        nodeToString(t),
        nodeToString(scaleBy),
      )
    | `Normalize(t) => "normalize(" ++ nodeToString(t) ++ ")"
    | `FloatFromDist(floatFromDistOp, t) =>
      SymbolicTypes.DistToFloat.format(floatFromDistOp, nodeToString(t))
    | `Truncate(lc, rc, t) => truncateToString(lc, rc, nodeToString(t))
    | `Render(t) => nodeToString(t);
};

module TreeNode = {
  type t = treeNode;
  type tResult = treeNode => result(treeNode, string);

  let rec toString =
    fun
    | `Leaf(`SymbolicDist(d)) => SymbolicDist.T.toString(d)
    | `Leaf(`RenderedDist(_)) => "[shape]"
    | `Operation(op) => Operation.toString(toString, op);

  /* The following modules encapsulate everything we can do with
   * different kinds of operations. */

  /* Given two random variables A and B, this returns the distribution
     of a new variable that is the result of the operation on A and B.
     For instance, normal(0, 1) + normal(1, 1) -> normal(1, 2).
     In general, this is implemented via convolution. */
  module AlgebraicCombination = {
    let toTreeNode = (op, t1, t2) =>
      `Operation(`AlgebraicCombination((op, t1, t2)));
    let tryAnalyticalSolution =
      fun
      | `Operation(
          `AlgebraicCombination(
            operation,
            `Leaf(`SymbolicDist(d1)),
            `Leaf(`SymbolicDist(d2)),
          ),
        ) as t =>
        switch (SymbolicDist.T.attemptAnalyticalOperation(d1, d2, operation)) {
        | `AnalyticalSolution(symbolicDist) =>
          Ok(`Leaf(`SymbolicDist(symbolicDist)))
        | `Error(er) => Error(er)
        | `NoSolution => Ok(t)
        }
      | t => Ok(t);

    // todo: I don't like the name evaluateNumerically that much, if this renders and does it algebraically. It's tricky.
    let evaluateNumerically = (algebraicOp, operationToLeaf, t1, t2) => {
      // force rendering into shapes
      let renderShape = r => operationToLeaf(`Render(r));
      switch (renderShape(t1), renderShape(t2)) {
      | (Ok(`Leaf(`RenderedDist(s1))), Ok(`Leaf(`RenderedDist(s2)))) =>
        Ok(
          `Leaf(
            `RenderedDist(
              Distributions.Shape.combineAlgebraically(algebraicOp, s1, s2),
            ),
          ),
        )
      | (Error(e1), _) => Error(e1)
      | (_, Error(e2)) => Error(e2)
      | _ => Error("Could not render shapes.")
      };
    };

    let evaluateToLeaf =
        (
          algebraicOp: SymbolicTypes.algebraicOperation,
          operationToLeaf,
          t1: t,
          t2: t,
        )
        : result(treeNode, string) =>
      algebraicOp
      |> toTreeNode(_, t1, t2)
      |> tryAnalyticalSolution
      |> E.R.bind(
           _,
           fun
           | `Leaf(d) => Ok(`Leaf(d)) // the analytical simplifaction worked, nice!
           | `Operation(_) =>
             // if not, run the convolution
             evaluateNumerically(algebraicOp, operationToLeaf, t1, t2),
         );
  };

  module VerticalScaling = {
    let evaluateToLeaf = (scaleOp, operationToLeaf, t, scaleBy) => {
      // scaleBy has to be a single float, otherwise we'll return an error.
      let fn = SymbolicTypes.Scale.toFn(scaleOp);
      let knownIntegralSumFn =
        SymbolicTypes.Scale.toKnownIntegralSumFn(scaleOp);
      let renderedShape = operationToLeaf(`Render(t));

      switch (renderedShape, scaleBy) {
      | (Ok(`Leaf(`RenderedDist(rs))), `Leaf(`SymbolicDist(`Float(sm)))) =>
        Ok(
          `Leaf(
            `RenderedDist(
              Distributions.Shape.T.mapY(
                ~knownIntegralSumFn=knownIntegralSumFn(sm),
                fn(sm),
                rs,
              ),
            ),
          ),
        )
      | (Error(e1), _) => Error(e1)
      | (_, _) => Error("Can only scale by float values.")
      };
    };
  };

  module PointwiseCombination = {
    let pointwiseAdd = (operationToLeaf, t1, t2) => {
      let renderedShape1 = operationToLeaf(`Render(t1));
      let renderedShape2 = operationToLeaf(`Render(t2));

      switch (renderedShape1, renderedShape2) {
      | (Ok(`Leaf(`RenderedDist(rs1))), Ok(`Leaf(`RenderedDist(rs2)))) =>
        Ok(
          `Leaf(
            `RenderedDist(
              Distributions.Shape.combinePointwise(
                ~knownIntegralSumsFn=(a, b) => Some(a +. b),
                (+.),
                rs1,
                rs2,
              ),
            ),
          ),
        )
      | (Error(e1), _) => Error(e1)
      | (_, Error(e2)) => Error(e2)
      | _ => Error("Could not perform pointwise addition.")
      };
    };

    let pointwiseMultiply = (operationToLeaf, t1, t2) => {
      // TODO: construct a function that we can easily sample from, to construct
      // a RenderedDist. Use the xMin and xMax of the rendered shapes to tell the sampling function where to look.
      Error(
        "Pointwise multiplication not yet supported.",
      );
    };

    let evaluateToLeaf = (pointwiseOp, operationToLeaf, t1, t2) => {
      switch (pointwiseOp) {
      | `Add => pointwiseAdd(operationToLeaf, t1, t2)
      | `Multiply => pointwiseMultiply(operationToLeaf, t1, t2)
      };
    };
  };

  module Truncate = {
    module Simplify = {
      let tryTruncatingNothing: tResult =
        fun
        | `Operation(`Truncate(None, None, `Leaf(d))) => Ok(`Leaf(d))
        | t => Ok(t);

      let tryTruncatingUniform: tResult =
        fun
        | `Operation(`Truncate(lc, rc, `Leaf(`SymbolicDist(`Uniform(u))))) => {
            // just create a new Uniform distribution
            let newLow = max(E.O.default(neg_infinity, lc), u.low);
            let newHigh = min(E.O.default(infinity, rc), u.high);
            Ok(
              `Leaf(`SymbolicDist(`Uniform({low: newLow, high: newHigh}))),
            );
          }
        | t => Ok(t);

      let attempt = (leftCutoff, rightCutoff, t): result(treeNode, string) => {
        let originalTreeNode =
          `Operation(`Truncate((leftCutoff, rightCutoff, t)));

        originalTreeNode
        |> tryTruncatingNothing
        |> E.R.bind(_, tryTruncatingUniform);
      };
    };

    let evaluateNumerically = (leftCutoff, rightCutoff, operationToLeaf, t) => {
      // TODO: use named args in renderToShape; if we're lucky we can at least get the tail
      // of a distribution we otherwise wouldn't get at all
      let renderedShape = operationToLeaf(`Render(t));

      switch (renderedShape) {
      | Ok(`Leaf(`RenderedDist(rs))) =>
        let truncatedShape =
          rs |> Distributions.Shape.T.truncate(leftCutoff, rightCutoff);
        Ok(`Leaf(`RenderedDist(rs)));
      | Error(e1) => Error(e1)
      | _ => Error("Could not truncate distribution.")
      };
    };

    let evaluateToLeaf =
        (
          leftCutoff: option(float),
          rightCutoff: option(float),
          operationToLeaf,
          t: treeNode,
        )
        : result(treeNode, string) => {
      t
      |> Simplify.attempt(leftCutoff, rightCutoff)
      |> E.R.bind(
           _,
           fun
           | `Leaf(d) => Ok(`Leaf(d)) // the analytical simplifaction worked, nice!
           | `Operation(_) =>
             evaluateNumerically(leftCutoff, rightCutoff, operationToLeaf, t),
         ); // if not, run the convolution
    };
  };

  module Normalize = {
    let rec evaluateToLeaf =
            (operationToLeaf, t: treeNode): result(treeNode, string) => {
      switch (t) {
      | `Leaf(`SymbolicDist(_)) => Ok(t)
      | `Leaf(`RenderedDist(s)) =>
        let normalized = Distributions.Shape.T.normalize(s);
        Ok(`Leaf(`RenderedDist(normalized)));
      | `Operation(op) =>
        E.R.bind(operationToLeaf(op), evaluateToLeaf(operationToLeaf))
      };
    };
  };

  module FloatFromDist = {
    let evaluateFromSymbolic = (distToFloatOp: distToFloatOperation, s) => {
      SymbolicDist.T.operate(distToFloatOp, s)
      |> E.R.bind(_, v => Ok(`Leaf(`SymbolicDist(`Float(v)))));
    };
    let evaluateFromRenderedDist =
        (distToFloatOp: distToFloatOperation, rs: DistTypes.shape)
        : result(treeNode, string) => {
      Distributions.Shape.operate(distToFloatOp, rs)
      |> (v => Ok(`Leaf(`SymbolicDist(`Float(v)))));
    };
    let rec evaluateToLeaf =
            (
              distToFloatOp: distToFloatOperation,
              operationToLeaf,
              t: treeNode,
            )
            : result(treeNode, string) => {
      switch (t) {
      | `Leaf(`SymbolicDist(s)) => evaluateFromSymbolic(distToFloatOp, s) // we want to evaluate the distToFloatOp on the symbolic dist
      | `Leaf(`RenderedDist(rs)) =>
        evaluateFromRenderedDist(distToFloatOp, rs)
      | `Operation(op) =>
        E.R.bind(
          operationToLeaf(op),
          evaluateToLeaf(distToFloatOp, operationToLeaf),
        )
      };
    };
  };

  module Render = {
    let rec evaluateToRenderedDist =
            (
              operationToLeaf: operation => result(t, string),
              sampleCount: int,
              t: treeNode,
            )
            : result(t, string) => {
      switch (t) {
      | `Leaf(`RenderedDist(s)) => Ok(`Leaf(`RenderedDist(s))) // already a rendered shape, we're done here
      | `Leaf(`SymbolicDist(d)) =>
        // todo: move to dist
        switch (d) {
        | `Float(v) =>
          Ok(
            `Leaf(
              `RenderedDist(
                Discrete(
                  Distributions.Discrete.make(
                    {xs: [|v|], ys: [|1.0|]},
                    Some(1.0),
                  ),
                ),
              ),
            ),
          )
        | _ =>
          let xs =
            SymbolicDist.T.interpolateXs(
              ~xSelection=`ByWeight,
              d,
              sampleCount,
            );
          let ys = xs |> E.A.fmap(x => SymbolicDist.T.pdf(x, d));
          Ok(
            `Leaf(
              `RenderedDist(
                Continuous(
                  Distributions.Continuous.make(
                    `Linear,
                    {xs, ys},
                    Some(1.0),
                  ),
                ),
              ),
            ),
          );
        }
      | `Operation(op) =>
        E.R.bind(
          operationToLeaf(op),
          evaluateToRenderedDist(operationToLeaf, sampleCount),
        )
      };
    };
  };

  let rec operationToLeaf =
          (sampleCount: int, op: operation): result(t, string) => {
    // the functions that convert the Operation nodes to Leaf nodes need to
    // have a way to call this function on their children, if their children are themselves Operation nodes.
    switch (op) {
    | `AlgebraicCombination(algebraicOp, t1, t2) =>
      AlgebraicCombination.evaluateToLeaf(
        algebraicOp,
        operationToLeaf(sampleCount),
        t1,
        t2 // we want to give it the option to render or simply leave it as is
      )
    | `PointwiseCombination(pointwiseOp, t1, t2) =>
      PointwiseCombination.evaluateToLeaf(
        pointwiseOp,
        operationToLeaf(sampleCount),
        t1,
        t2,
      )
    | `VerticalScaling(scaleOp, t, scaleBy) =>
      VerticalScaling.evaluateToLeaf(
        scaleOp,
        operationToLeaf(sampleCount),
        t,
        scaleBy,
      )
    | `Truncate(leftCutoff, rightCutoff, t) =>
      Truncate.evaluateToLeaf(
        leftCutoff,
        rightCutoff,
        operationToLeaf(sampleCount),
        t,
      )
    | `FloatFromDist(distToFloatOp, t) =>
      FloatFromDist.evaluateToLeaf(
        distToFloatOp,
        operationToLeaf(sampleCount),
        t,
      )
    | `Normalize(t) =>
      Normalize.evaluateToLeaf(operationToLeaf(sampleCount), t)
    | `Render(t) =>
      Render.evaluateToRenderedDist(
        operationToLeaf(sampleCount),
        sampleCount,
        t,
      )
    };
  };

  /* This function recursively goes through the nodes of the parse tree,
     replacing each Operation node and its subtree with a Data node.
     Whenever possible, the replacement produces a new Symbolic Data node,
     but most often it will produce a RenderedDist.
     This function is used mainly to turn a parse tree into a single RenderedDist
     that can then be displayed to the user. */
  let toLeaf = (treeNode: t, sampleCount: int): result(t, string) => {
    switch (treeNode) {
    | `Leaf(d) => Ok(`Leaf(d))
    | `Operation(op) => operationToLeaf(sampleCount, op)
    };
  };
};

let toShape = (sampleCount: int, treeNode: treeNode) => {
  let renderResult =
    TreeNode.toLeaf(`Operation(`Render(treeNode)), sampleCount);

  switch (renderResult) {
  | Ok(`Leaf(`RenderedDist(rs))) =>
    let continuous = Distributions.Shape.T.toContinuous(rs);
    let discrete = Distributions.Shape.T.toDiscrete(rs);
    let shape = MixedShapeBuilder.buildSimple(~continuous, ~discrete);
    shape |> E.O.toExt("Could not build final shape.");
  | Ok(_) => E.O.toExn("Rendering failed.", None)
  | Error(message) => E.O.toExn("No shape found, error: " ++ message, None)
  };
};

let toString = (treeNode: treeNode) => TreeNode.toString(treeNode);
