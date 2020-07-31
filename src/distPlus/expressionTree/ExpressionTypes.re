type algebraicOperation = [ | `Add | `Multiply | `Subtract | `Divide | `Exponentiate];
type pointwiseOperation = [ | `Add | `Multiply | `Exponentiate];
type scaleOperation = [ | `Multiply | `Exponentiate | `Log];
type distToFloatOperation = [
  | `Pdf(float)
  | `Cdf(float)
  | `Inv(float)
  | `Mean
  | `Sample
];

module ExpressionTree = {
  type node = [
    | `SymbolicDist(SymbolicTypes.symbolicDist)
    | `RenderedDist(DistTypes.shape)
    | `AlgebraicCombination(algebraicOperation, node, node)
    | `PointwiseCombination(pointwiseOperation, node, node)
    | `VerticalScaling(scaleOperation, node, node)
    | `Render(node)
    | `Truncate(option(float), option(float), node)
    | `Normalize(node)
    | `FloatFromDist(distToFloatOperation, node)
    | `Function(array(string), node)
    | `FunctionCall(string, array(node))
    | `Symbol(string)
  ];

  type samplingInputs = {
    sampleCount: int,
    outputXYPoints: int,
    kernelWidth: option(float),
    shapeLength: int
  };

  type environment = Belt.Map.String.t(node);

  module Environment = {
    type t = environment
    module MS = Belt.Map.String;
    let fromArray = MS.fromArray
    let empty:t = [||]->fromArray;
    let mergeKeepSecond = (a:t,b:t) => MS.merge(a,b, (_,a,b) =>switch(a,b){
      | (_, Some(b)) => Some(b)
      | (Some(a), _) => Some(a)
      | _ => None
    })
    let update = (t,str, fn) => MS.update(t, str, fn)
    let get = (t:t,str) => MS.get(t, str)
  }

  type evaluationParams = {
    samplingInputs,
    environment,
    evaluateNode: (evaluationParams, node) => Belt.Result.t(node, string),
  };

  let evaluateNode = (evaluationParams: evaluationParams) =>
    evaluationParams.evaluateNode(evaluationParams);

  let evaluateAndRetry = (evaluationParams, fn, node) =>
    node
    |> evaluationParams.evaluateNode(evaluationParams)
    |> E.R.bind(_, fn(evaluationParams));

  module Render = {
    type t = node;

    let render = (evaluationParams: evaluationParams, r) =>
      `Render(r) |> evaluateNode(evaluationParams);

    let ensureIsRendered = (params, t) =>
      switch (t) {
      | `RenderedDist(_) => Ok(t)
      | _ =>
        switch (render(params, t)) {
        | Ok(`RenderedDist(r)) => Ok(`RenderedDist(r))
        | Ok(_) => Error("Did not render as requested")
        | Error(e) => Error(e)
        }
      };

    let ensureIsRenderedAndGetShape = (params, t) =>
      switch (ensureIsRendered(params, t)) {
      | Ok(`RenderedDist(r)) => Ok(r)
      | Ok(_) => Error("Did not render as requested")
      | Error(e) => Error(e)
      };

    let getShape = (item: node) =>
      switch (item) {
      | `RenderedDist(r) => Some(r)
      | _ => None
      };

    let _toFloat = (t: DistTypes.shape) =>
      switch (t) {
      | Discrete({xyShape: {xs: [|x|], ys: [|1.0|]}}) =>
        Some(`SymbolicDist(`Float(x)))
      | _ => None
      };

    let toFloat = (item: node): result(node, string) =>
      item
      |> getShape
      |> E.O.bind(_, _toFloat)
      |> E.O.toResult("Not valid shape");
  };
};

type simplificationResult = [
  | `Solution(ExpressionTree.node)
  | `Error(string)
  | `NoSolution
];

module Program = {
  type statement = [ | `Assignment(string, ExpressionTree.node) | `Expression(ExpressionTree.node)];
  type program = array(statement);
}