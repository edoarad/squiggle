type node = ExpressionTypes.ExpressionTree.node;
let getFloat = ExpressionTypes.ExpressionTree.getFloat;

type samplingDist = [
  | `SymbolicDist(SymbolicTypes.symbolicDist)
  | `RenderedDist(DistTypes.shape)
];

type _type = [
  | `Float
  | `SamplingDistribution
  | `RenderedDistribution
  | `Array(_type)
  | `Named(array((string, _type)))
];

type typedValue = [
  | `Float(float)
  | `RenderedDist(DistTypes.shape)
  | `SamplingDist(samplingDist)
  | `Array(array(typedValue))
  | `Named(array((string, typedValue)))
];

type _function = {
  name: string,
  inputTypes: array(_type),
  outputType: _type,
  run: array(typedValue) => result(node, string),
  shouldCoerceTypes: bool,
};

type functions = array(_function);
type inputNodes = array(node);

module TypedValue = {
  let rec fromNode = (node: node): result(typedValue, string) =>
    switch (ExpressionTypes.ExpressionTree.toFloatIfNeeded(node)) {
    | `SymbolicDist(`Float(r)) => Ok(`Float(r))
    | `SymbolicDist(s) => Ok(`SamplingDist(`SymbolicDist(s)))
    | `RenderedDist(s) => Ok(`RenderedDist(s))
    | `Array(r) =>
      r
      |> E.A.fmap(fromNode)
      |> E.A.R.firstErrorOrOpen
      |> E.R.fmap(r => `Array(r))
    | `Hash(hash) =>
      hash
      |> E.A.fmap(((name, t)) => fromNode(t) |> E.R.fmap(r => (name, r)))
      |> E.A.R.firstErrorOrOpen
      |> E.R.fmap(r => `Named(r))
    | _ => Error("Wrong type")
    };

  // todo: Arrays and hashes
  let rec fromNodeWithTypeCoercion = (evaluationParams, _type: _type, node) => {
    switch (_type, node) {
    | (`Float, _) =>
      switch (getFloat(node)) {
      | Some(a) => Ok(`Float(a))
      | _ => Error("Type Error: Expected float.")
      }
    | (`SamplingDistribution, _) =>
      PTypes.SamplingDistribution.renderIfIsNotSamplingDistribution(
        evaluationParams,
        node,
      )
      |> E.R.bind(_, fromNode)
    | (`RenderedDistribution, _) =>
      ExpressionTypes.ExpressionTree.Render.render(evaluationParams, node)
      |> E.R.bind(_, fromNode)
    | (`Array(_type), `Array(b)) =>
      b
      |> E.A.fmap(fromNodeWithTypeCoercion(evaluationParams, _type))
      |> E.A.R.firstErrorOrOpen
      |> E.R.fmap(r => `Array(r))
    | (`Named(named), `Hash(r)) =>
      let foo =
        named
        |> E.A.fmap(((name, intendedType)) =>
             (
               name,
               intendedType,
               ExpressionTypes.ExpressionTree.Hash.getByName(r, name),
             )
           );
      let bar =
        foo
        |> E.A.fmap(((name, intendedType, optionNode)) =>
             switch (optionNode) {
             | Some(node) =>
               fromNodeWithTypeCoercion(evaluationParams, intendedType, node)
               |> E.R.fmap(node => (name, node))
             | None => Error("Hash parameter not present in hash.")
             }
           )
        |> E.A.R.firstErrorOrOpen
        |> E.R.fmap(r => `Named(r));
      bar;
    | _ => Error("fromNodeWithTypeCoercion error, sorry.")
    };
  };

  let toFloat =
    fun
    | `Float(x) => Ok(x)
    | _ => Error("Not a float");

  let toArray =
    fun
    | `Array(x) => Ok(x)
    | _ => Error("Not an array");

  let toNamed =
    fun
    | `Named(x) => Ok(x)
    | _ => Error("Not a named item");

  let toDist =
    fun
    | `SamplingDist(`SymbolicDist(c)) => Ok(`SymbolicDist(c))
    | `SamplingDist(`RenderedDist(c)) => Ok(`RenderedDist(c))
    | `Float(x) =>
      Ok(`RenderedDist(SymbolicDist.T.toShape(1000, `Float(x))))
    | _ => Error("");
};

module Function = {
  type t = _function;
  type ts = functions;

  module T = {
    let make =
        (~name, ~inputTypes, ~outputType, ~run, ~shouldCoerceTypes=true, _): t => {
      name,
      inputTypes,
      outputType,
      run,
      shouldCoerceTypes,
    };

    let _inputLengthCheck = (inputNodes: inputNodes, t: t) => {
      let expectedLength = E.A.length(t.inputTypes);
      let actualLength = E.A.length(inputNodes);
      expectedLength == actualLength
        ? Ok(inputNodes)
        : Error(
            "Wrong number of inputs. Expected"
            ++ (expectedLength |> E.I.toString)
            ++ ". Got:"
            ++ (actualLength |> E.I.toString),
          );
    };

    let _coerceInputNodes =
        (evaluationParams, inputTypes, shouldCoerce, inputNodes) =>
      Belt.Array.zip(inputTypes, inputNodes)
      |> E.A.fmap(((def, input)) =>
           shouldCoerce
             ? TypedValue.fromNodeWithTypeCoercion(
                 evaluationParams,
                 def,
                 input,
               )
             : TypedValue.fromNode(input)
         )
      |> E.A.R.firstErrorOrOpen;

    let inputsToTypedValues =
        (
          evaluationParams: ExpressionTypes.ExpressionTree.evaluationParams,
          inputNodes: inputNodes,
          t: t,
        ) => {
      _inputLengthCheck(inputNodes, t)
      ->E.R.bind(
          _coerceInputNodes(
            evaluationParams,
            t.inputTypes,
            t.shouldCoerceTypes,
          ),
        );
    };

    let run =
        (
          evaluationParams: ExpressionTypes.ExpressionTree.evaluationParams,
          inputNodes: inputNodes,
          t: t,
        ) => {
      Js.log("Running!");
      inputsToTypedValues(evaluationParams, inputNodes, t)->E.R.bind(t.run)
      |> (
        fun
        | Ok(i) => Ok(i)
        | Error(r) => {
            Error("Function " ++ t.name ++ " error: " ++ r);
          }
      );
    };
  };

  module Ts = {
    let findByName = (ts: ts, n: string) =>
      ts |> Belt.Array.getBy(_, ({name}) => name == n);

    let findByNameAndRun = (ts: ts, n: string, evaluationParams, inputTypes) =>
      findByName(ts, n) |> E.O.fmap(T.run(evaluationParams, inputTypes));
  };
};
