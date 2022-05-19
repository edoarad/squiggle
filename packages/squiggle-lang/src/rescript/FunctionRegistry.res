type expressionValue = ReducerInterface_ExpressionValue.expressionValue

type rec itype =
  | I_Number
  | I_Numeric
  | I_DistOrNumber
  | I_Record(iRecord)
  | I_Array(array<itype>)
  | I_Option(itype)
and iRecord = array<iRecordParam>
and iRecordParam = (string, itype)

type rec value =
  | Number(float)
  | Dist(DistributionTypes.genericDist)
  | Option(option<value>)
  | DistOrNumber(distOrNumber)
  | Record(record)
and record = array<(string, value)>
and distOrNumber = Number(float) | Dist(DistributionTypes.genericDist)

type runFn = array<value> => result<expressionValue, string>

type fnDefinition = {name: string, inputs: array<itype>, run: runFn}

type function = {
  name: string,
  definitions: array<fnDefinition>,
}

type registry = array<function>

let rec matchInput = (input: itype, r: expressionValue): option<value> =>
  switch (input, r) {
  | (I_Number, EvNumber(f)) => Some(Number(f))
  | (I_DistOrNumber, EvNumber(f)) => Some(DistOrNumber(Number(f)))
  | (I_DistOrNumber, EvDistribution(Symbolic(#Float(f)))) => Some(DistOrNumber(Number(f)))
  | (I_DistOrNumber, EvDistribution(f)) => Some(DistOrNumber(Dist(f)))
  | (I_Numeric, EvNumber(f)) => Some(Number(f))
  | (I_Numeric, EvDistribution(Symbolic(#Float(f)))) => Some(Number(f))
  | (I_Option(v), _) => Some(Option(matchInput(v, r)))
  | (I_Record(recordParams), EvRecord(record)) => {
      let getAndMatch = (name, input) =>
        E.Dict.get(record, name)->E.O.bind(v => matchInput(input, v))
      let arrayOfNameValues: array<(Js.Dict.key, option<value>)> =
        recordParams->E.A2.fmap(((name, input)) => (name, getAndMatch(name, input)))
      let hasNullValues = E.A.hasBy(arrayOfNameValues, ((_, value)) => E.O.isNone(value))
      if hasNullValues {
        None
      } else {
        arrayOfNameValues
        ->E.A2.fmap(((name, value)) => (name, value->E.O2.toExn("")))
        ->(r => Some(Record(r)))
      }
    }
  | _ => None
  }

module MatchSimple = {
  type t = DifferentName | SameNameDifferentArguments | FullMatch

  let isFullMatch = (match: t) =>
    switch match {
    | FullMatch => true
    | _ => false
    }

  let isNameMatchOnly = (match: t) =>
    switch match {
    | SameNameDifferentArguments => true
    | _ => false
    }
}

module Match = {
  type t<'a, 'b> = DifferentName | SameNameDifferentArguments('a) | FullMatch('b)

  let isFullMatch = (match: t<'a, 'b>): bool =>
    switch match {
    | FullMatch(_) => true
    | _ => false
    }

  let isNameMatchOnly = (match: t<'a, 'b>) =>
    switch match {
    | SameNameDifferentArguments(_) => true
    | _ => false
    }
}

module FnDefinition = {
  type definitionMatch = MatchSimple.t

  let getArgValues = (f: fnDefinition, args: array<expressionValue>): option<array<value>> => {
    let inputTypes = f.inputs
    if E.A.length(f.inputs) !== E.A.length(args) {
      None
    } else {
      E.A.zip(inputTypes, args)
      ->E.A2.fmap(((input, arg)) => matchInput(input, arg))
      ->E.A.O.openIfAllSome
    }
  }

  let matchAssumingSameName = (f: fnDefinition, args: array<expressionValue>) => {
    switch getArgValues(f, args) {
    | Some(_) => MatchSimple.FullMatch
    | None => MatchSimple.SameNameDifferentArguments
    }
  }

  let match = (f: fnDefinition, fnName: string, args: array<expressionValue>) => {
    if f.name !== fnName {
      MatchSimple.DifferentName
    } else {
      matchAssumingSameName(f, args)
    }
  }

  let run = (f: fnDefinition, args: array<expressionValue>) => {
    let argValues = getArgValues(f, args)
    switch argValues {
    | Some(values) => f.run(values)
    | None => Error("Impossible")
    }
  }
}

module Function = {
  type definitionId = int
  type match = Match.t<array<definitionId>, definitionId>

  let make = (name, definitions): function => {
    name: name,
    definitions: definitions,
  }
  let makeDefinition = (name, inputs, run): fnDefinition => {
    name: name,
    inputs: inputs,
    run: run,
  }

  let match = (f: function, fnName: string, args: array<expressionValue>): match => {
    let matchedDefinition = () =>
      E.A.getIndexBy(f.definitions, r =>
        MatchSimple.isFullMatch(FnDefinition.match(r, fnName, args))
      ) |> E.O.fmap(r => Match.FullMatch(r))
    let getMatchedNameOnlyDefinition = () => {
      let nameMatchIndexes =
        f.definitions
        ->E.A2.fmapi((index, r) =>
          MatchSimple.isNameMatchOnly(FnDefinition.match(r, fnName, args)) ? Some(index) : None
        )
        ->E.A.O.concatSomes
      switch nameMatchIndexes {
      | [] => None
      | elements => Some(Match.SameNameDifferentArguments(elements))
      }
    }

    E.A.O.firstSomeFnWithDefault(
      [matchedDefinition, getMatchedNameOnlyDefinition],
      Match.DifferentName,
    )
  }
}

module RegistryMatch = {
  type match = {
    fnName: string,
    inputIndex: int,
  }
  type t = Match.t<array<match>, match>
  let makeMatch = (fnName: string, inputIndex: int) => {fnName: fnName, inputIndex: inputIndex}
}

module Registry = {
  let findExactMatches = (r: registry, fnName: string, args: array<expressionValue>) => {
    let functionMatchPairs = r->E.A2.fmap(l => (l, Function.match(l, fnName, args)))
    let getFullMatch = E.A.getBy(functionMatchPairs, ((_, match: Function.match)) =>
      Match.isFullMatch(match)
    )
    let fullMatch: option<RegistryMatch.match> = getFullMatch->E.O.bind(((fn, match)) =>
      switch match {
      | FullMatch(index) => Some(RegistryMatch.makeMatch(fn.name, index))
      | _ => None
      }
    )
    fullMatch
  }

  let findNameMatches = (r: registry, fnName: string, args: array<expressionValue>) => {
    let functionMatchPairs = r->E.A2.fmap(l => (l, Function.match(l, fnName, args)))
    let getNameMatches =
      functionMatchPairs
      ->E.A2.fmap(((fn, match)) => Match.isNameMatchOnly(match) ? Some((fn, match)) : None)
      ->E.A.O.concatSomes
    let matches =
      getNameMatches
      ->E.A2.fmap(((fn, match)) =>
        switch match {
        | SameNameDifferentArguments(indexes) =>
          indexes->E.A2.fmap(index => RegistryMatch.makeMatch(fn.name, index))
        | _ => []
        }
      )
      ->Belt.Array.concatMany
    E.A.toNoneIfEmpty(matches)
  }

  let findMatches = (r: registry, fnName: string, args: array<expressionValue>) => {
    switch findExactMatches(r, fnName, args) {
    | Some(r) => Match.FullMatch(r)
    | None =>
      switch findNameMatches(r, fnName, args) {
      | Some(r) => Match.SameNameDifferentArguments(r)
      | None => Match.DifferentName
      }
    }
  }

  let fullMatchToDef = (registry: registry, {fnName, inputIndex}: RegistryMatch.match): option<
    fnDefinition,
  > =>
    registry
    ->E.A.getBy(fn => fn.name === fnName)
    ->E.O.bind(fn => E.A.get(fn.definitions, inputIndex))

  let matchAndRun = (r: registry, fnName: string, args: array<expressionValue>) => {
    switch findMatches(r, fnName, args) {
    | Match.FullMatch(m) =>
      fullMatchToDef(r, m)->E.O2.fmap(r => {
        FnDefinition.run(r, args)
      })
    | _ => None
    }
  }
}

let impossibleError = "Wrong inputs / Logically impossible"

let twoNumberInputs = (inputs: array<value>) => {
  switch inputs {
  | [Number(n1), Number(n2)] => Ok(n1, n2)
  | _ => Error(impossibleError)
  }
}

let twoNumberInputsRecord = (v1, v2, inputs: array<value>) =>
  switch inputs {
  | [Record([(name1, n1), (name2, n2)])] if name1 == v1 && name2 == v2 => twoNumberInputs([n1, n2])
  | _ => Error(impossibleError)
  }

let contain = r => ReducerInterface_ExpressionValue.EvDistribution(Symbolic(r))

let meanStdev = (mean, stdev) => SymbolicDist.Normal.make(mean, stdev)->E.R2.fmap(contain)

let p5and95 = (p5, p95) => contain(SymbolicDist.Normal.from90PercentCI(p5, p95))

let convertTwoInputs = (inputs: array<value>): result<expressionValue, string> =>
  twoNumberInputs(inputs)->E.R.bind(((mean, stdev)) => meanStdev(mean, stdev))

let twoDistOrStdev = (a1: value, a2: value) => {
  switch (a1, a2) {
  | (DistOrNumber(a1), DistOrNumber(a2)) => Ok(a1, a2)
  | _ => Error(impossibleError)
  }
}

let distTwo = (
  ~fn: (float, float) => result<DistributionTypes.genericDist, string>,
  a1: value,
  a2: value,
) => {
  let toSampleSet = r => GenericDist.toSampleSetDist(r, 1000)
  let sampleSetToExpressionValue = (
    b: Belt.Result.t<QuriSquiggleLang.SampleSetDist.t, QuriSquiggleLang.DistributionTypes.error>,
  ) =>
    switch b {
    | Ok(r) => Ok(ReducerInterface_ExpressionValue.EvDistribution(SampleSet(r)))
    | Error(d) => Error(DistributionTypes.Error.toString(d))
    }

  let mapFnResult = r =>
    switch r {
    | Ok(r) => Ok(GenericDist.sample(r))
    | Error(r) => Error(Operation.Other(r))
    }

  let singleVarSample = (a, fn) => {
    let sampleSetResult =
      toSampleSet(a) |> E.R2.bind(dist =>
        SampleSetDist.samplesMap(
          ~fn=f => fn(f)->mapFnResult,
          dist,
        )->E.R2.errMap(r => DistributionTypes.SampleSetError(r))
      )
    sampleSetResult->sampleSetToExpressionValue
  }

  switch (a1, a2) {
  | (DistOrNumber(Number(a1)), DistOrNumber(Number(a2))) =>
    fn(a1, a2)->E.R2.fmap(r => ReducerInterface_ExpressionValue.EvDistribution(r))
  | (DistOrNumber(Dist(a1)), DistOrNumber(Number(a2))) => singleVarSample(a1, r => fn(r, a2))
  | (DistOrNumber(Number(a1)), DistOrNumber(Dist(a2))) => singleVarSample(a2, r => fn(a1, r))
  | (DistOrNumber(Dist(a1)), DistOrNumber(Dist(a2))) => {
      let altFn = (a, b) => fn(a, b)->mapFnResult
      let sampleSetResult =
        E.R.merge(toSampleSet(a1), toSampleSet(a2))
        ->E.R2.errMap(DistributionTypes.Error.toString)
        ->E.R.bind(((t1, t2)) => {
          SampleSetDist.map2(~fn=altFn, ~t1, ~t2)->E.R2.errMap(Operation.Error.toString)
        })
        ->E.R2.errMap(r => DistributionTypes.OtherError(r))
      sampleSetResult->sampleSetToExpressionValue
    }
  | _ => Error(impossibleError)
  }
}

let normal = Function.make(
  "Normal",
  [
    Function.makeDefinition("normal", [I_DistOrNumber, I_DistOrNumber], inputs => {
      let combine = (a1: float, a2: float) =>
        SymbolicDist.Normal.make(a1, a2)->E.R2.fmap(r => DistributionTypes.Symbolic(r))
      distTwo(~fn=combine, inputs[0], inputs[1])
    }),
    Function.makeDefinition(
      "normal",
      [I_Record([("mean", I_Numeric), ("stdev", I_Numeric)])],
      inputs =>
        twoNumberInputsRecord("mean", "stdev", inputs)->E.R.bind(((mean, stdev)) =>
          meanStdev(mean, stdev)
        ),
    ),
    Function.makeDefinition("normal", [I_Record([("p5", I_Numeric), ("p95", I_Numeric)])], inputs =>
      twoNumberInputsRecord("p5", "p95", inputs)->E.R.bind(((v1, v2)) => Ok(p5and95(v1, v2)))
    ),
  ],
)

let logNormal = Function.make(
  "Lognormal",
  [
    Function.makeDefinition("lognormal", [I_Numeric, I_Numeric], inputs =>
      twoNumberInputs(inputs)->E.R.bind(((mu, sigma)) =>
        SymbolicDist.Lognormal.make(mu, sigma)->E.R2.fmap(contain)
      )
    ),
    Function.makeDefinition(
      "lognormal",
      [I_Record([("p5", I_Numeric), ("p95", I_Numeric)])],
      inputs =>
        twoNumberInputsRecord("p5", "p95", inputs)->E.R.bind(((p5, p95)) => Ok(
          contain(SymbolicDist.Lognormal.from90PercentCI(p5, p95)),
        )),
    ),
    Function.makeDefinition(
      "lognormal",
      [I_Record([("mean", I_Numeric), ("stdev", I_Numeric)])],
      inputs =>
        twoNumberInputsRecord("mean", "stdev", inputs)->E.R.bind(((mean, stdev)) =>
          SymbolicDist.Lognormal.fromMeanAndStdev(mean, stdev)->E.R2.fmap(contain)
        ),
    ),
  ],
)

let allFunctions = [normal, logNormal]
