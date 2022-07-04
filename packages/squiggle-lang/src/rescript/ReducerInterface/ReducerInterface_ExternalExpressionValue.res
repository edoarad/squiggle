/*
  Irreducible values. Reducer does not know about those. Only used for external calls
  This is a configuration to to make external calls of those types
*/
module Extra_Array = Reducer_Extra_Array
module ErrorValue = Reducer_ErrorValue
@genType.opaque
type internalCode = Object

@genType
type rec externalExpressionValue =
  | EvArray(array<externalExpressionValue>)
  | EvArrayString(array<string>)
  | EvBool(bool)
  | EvCall(string) // External function call
  | EvDistribution(DistributionTypes.genericDist)
  | EvLambda(lambdaValue)
  | EvNumber(float)
  | EvRecord(record)
  | EvString(string)
  | EvSymbol(string)
  | EvDate(Js.Date.t)
  | EvTimeDuration(float)
  | EvDeclaration(lambdaDeclaration)
  | EvTypeIdentifier(string)
  | EvModule(record)
and record = Js.Dict.t<externalExpressionValue>
and externalBindings = record
and lambdaValue = {
  parameters: array<string>,
  context: externalBindings,
  body: internalCode,
}
and lambdaDeclaration = Declaration.declaration<lambdaValue>

@genType
type t = externalExpressionValue

type functionCall = (string, array<externalExpressionValue>)

let rec toString = aValue =>
  switch aValue {
  | EvArray(anArray) => {
      let args = anArray->Js.Array2.map(each => toString(each))->Js.Array2.toString
      `[${args}]`
    }
  | EvArrayString(anArray) => {
      let args = anArray->Js.Array2.toString
      `[${args}]`
    }
  | EvBool(aBool) => Js.String.make(aBool)
  | EvCall(fName) => `:${fName}`
  | EvLambda(lambdaValue) => `lambda(${Js.Array2.toString(lambdaValue.parameters)}=>internal code)`
  | EvNumber(aNumber) => Js.String.make(aNumber)
  | EvString(aString) => `'${aString}'`
  | EvSymbol(aString) => `:${aString}`
  | EvRecord(aRecord) => aRecord->toStringRecord
  | EvDistribution(dist) => GenericDist.toString(dist)
  | EvDate(date) => DateTime.Date.toString(date)
  | EvTimeDuration(t) => DateTime.Duration.toString(t)
  | EvDeclaration(d) => Declaration.toString(d, r => toString(EvLambda(r)))
  | EvTypeIdentifier(id) => `#${id}`
  | EvModule(m) => `@${m->toStringRecord}`
  }
and toStringRecord = aRecord => {
  let pairs =
    aRecord
    ->Js.Dict.entries
    ->Js.Array2.map(((eachKey, eachValue)) => `${eachKey}: ${toString(eachValue)}`)
    ->Js.Array2.toString
  `{${pairs}}`
}

let argsToString = (args: array<externalExpressionValue>): string => {
  args->Js.Array2.map(arg => arg->toString)->Js.Array2.toString
}

let toStringFunctionCall = ((fn, args)): string => `${fn}(${argsToString(args)})`

let toStringResult = x =>
  switch x {
  | Ok(a) => `Ok(${toString(a)})`
  | Error(m) => `Error(${ErrorValue.errorToString(m)})`
  }

@genType
type environment = DistributionOperation.env

@genType
let defaultEnvironment: environment = DistributionOperation.defaultEnv

type expressionValueType =
  | EvtArray
  | EvtArrayString
  | EvtBool
  | EvtCall
  | EvtDistribution
  | EvtLambda
  | EvtNumber
  | EvtRecord
  | EvtString
  | EvtSymbol
  | EvtDate
  | EvtTimeDuration
  | EvtDeclaration
  | EvtTypeIdentifier
  | EvtModule

type functionCallSignature = CallSignature(string, array<expressionValueType>)
type functionDefinitionSignature =
  FunctionDefinitionSignature(functionCallSignature, expressionValueType)

let valueToValueType = value =>
  switch value {
  | EvArray(_) => EvtArray
  | EvArrayString(_) => EvtArrayString
  | EvBool(_) => EvtBool
  | EvCall(_) => EvtCall
  | EvDistribution(_) => EvtDistribution
  | EvLambda(_) => EvtLambda
  | EvNumber(_) => EvtNumber
  | EvRecord(_) => EvtRecord
  | EvString(_) => EvtString
  | EvSymbol(_) => EvtSymbol
  | EvDate(_) => EvtDate
  | EvTimeDuration(_) => EvtTimeDuration
  | EvDeclaration(_) => EvtDeclaration
  | EvTypeIdentifier(_) => EvtTypeIdentifier
  | EvModule(_) => EvtModule
  }

let functionCallToCallSignature = (functionCall: functionCall): functionCallSignature => {
  let (fn, args) = functionCall
  CallSignature(fn, args->Js.Array2.map(valueToValueType))
}

let valueTypeToString = (valueType: expressionValueType): string =>
  switch valueType {
  | EvtArray => `Array`
  | EvtArrayString => `ArrayString`
  | EvtBool => `Bool`
  | EvtCall => `Call`
  | EvtDistribution => `Distribution`
  | EvtLambda => `Lambda`
  | EvtNumber => `Number`
  | EvtRecord => `Record`
  | EvtString => `String`
  | EvtSymbol => `Symbol`
  | EvtDate => `Date`
  | EvtTimeDuration => `Duration`
  | EvtDeclaration => `Declaration`
  | EvtTypeIdentifier => `TypeIdentifier`
  | EvtModule => `Module`
  }

let functionCallSignatureToString = (functionCallSignature: functionCallSignature): string => {
  let CallSignature(fn, args) = functionCallSignature
  `${fn}(${args->Js.Array2.map(valueTypeToString)->Js.Array2.toString})`
}