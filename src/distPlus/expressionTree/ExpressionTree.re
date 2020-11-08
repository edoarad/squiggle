open ExpressionTypes.ExpressionTree;

let rec toString: node => string =
  fun
  | `SymbolicDist(d) => SymbolicDist.T.toString(d)
  | `RenderedDist(_) => "[renderedShape]"
  | `AlgebraicCombination(op, t1, t2) =>
    Operation.Algebraic.format(op, toString(t1), toString(t2))
  | `PointwiseCombination(op, t1, t2) =>
    Operation.Pointwise.format(op, toString(t1), toString(t2))
  | `Normalize(t) => "normalize(k" ++ toString(t) ++ ")"
  | `Truncate(lc, rc, t) =>
    Operation.T.truncateToString(lc, rc, toString(t))
  | `Render(t) => toString(t)
  | `Symbol(t) => "Symbol: " ++ t
  | `FunctionCall(name, args) =>
    "[Function call: ("
    ++ name
    ++ (args |> E.A.fmap(toString) |> Js.String.concatMany(_, ","))
    ++ ")]"
  | `Function(args, internal) =>
    "[Function: ("
    ++ (args |> Js.String.concatMany(_, ","))
    ++ toString(internal)
    ++ ")]"
  | `Array(_) => "Array"
  | `Hash(_) => "Hash"

let envs = (samplingInputs, environment) => {
  {samplingInputs, environment, evaluateNode: ExpressionTreeEvaluator.toLeaf};
};

let toLeaf = (samplingInputs, environment, node: node) =>
  ExpressionTreeEvaluator.toLeaf(envs(samplingInputs, environment), node);
let toShape = (samplingInputs, environment, node: node) => {
  switch (toLeaf(samplingInputs, environment, node)) {
  | Ok(`RenderedDist(shape)) => Ok(shape)
  | Ok(_) => Error("Rendering failed.")
  | Error(e) => Error(e)
  };
};

let runFunction = (samplingInputs, environment, inputs, fn: PTypes.Function.t) => {
  let params = envs(samplingInputs, environment);
  PTypes.Function.run(params, inputs, fn)
}
