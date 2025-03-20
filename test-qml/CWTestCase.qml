import QtQuick
import QtTest

TestCase {
    function tryFuzzyCompare(actual, expected, delta, message = "") {
        let err = new Error();
        let extraMessage = "actual:" + actual + " expected:" + expected + " ActualDelta:" + Math.abs(actual - expected) + " ExpectedDelta:" + delta
        tryVerify(() => {
                       return Math.abs(actual - expected) <= delta;
                  },
                  5000,
                  message + "\nCompare:" + extraMessage + "\nat:" + err.stack);
    }
}
