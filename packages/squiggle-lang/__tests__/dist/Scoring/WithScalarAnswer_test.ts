import {
  logScoreScalarAnswer,
  mixture,
} from "../../../src/dist/DistOperations";
import { env, mkDelta, unpackResult } from "../../helpers/distHelpers";

describe("WithScalarAnswer: discrete -> scalar -> score", () => {
  const pointA = mkDelta(3.0);
  const pointB = mkDelta(2.0);
  const pointC = mkDelta(1.0);
  const pointD = mkDelta(0.0);

  test("score: agrees with analytical answer when finite", () => {
    const prediction = unpackResult(
      mixture(
        [
          [pointA, 0.25],
          [pointB, 0.25],
          [pointC, 0.25],
          [pointD, 0.25],
        ],
        { env }
      )
    );

    const answer = 2.0; // So this is: assigning 100% probability to 2.0
    const x = unpackResult(
      logScoreScalarAnswer({
        estimate: prediction,
        answer,
        prior: undefined,
        env,
      })
    );
    expect(x).toEqual(-Math.log(0.25 / 1.0));
  });

  test("score: agrees with analytical answer when finite", () => {
    const prediction = unpackResult(
      mixture(
        [
          [pointA, 0.75],
          [pointB, 0.25],
        ],
        { env }
      )
    );

    const answer = 3.0; // So this is: assigning 100% probability to 2.0
    const x = unpackResult(
      logScoreScalarAnswer({
        estimate: prediction,
        answer,
        prior: undefined,
        env,
      })
    );
    expect(x).toEqual(-Math.log(0.75 / 1.0));
  });

  test("scoreWithPrior: agrees with analytical answer when finite", () => {
    const prior = unpackResult(
      mixture(
        [
          [pointA, 0.5],
          [pointB, 0.5],
        ],
        { env }
      )
    );
    const prediction = unpackResult(
      mixture(
        [
          [pointA, 0.75],
          [pointB, 0.25],
        ],
        { env }
      )
    );

    const answer = 3.0; // So this is: assigning 100% probability to 2.0
    const x = unpackResult(
      logScoreScalarAnswer({
        estimate: prediction,
        answer,
        prior,
        env,
      })
    );
    expect(x).toEqual(-Math.log(0.75 / 1.0) - -Math.log(0.5 / 1.0));
  });
});