import QtQuick
import QtTest

TestCase {
    //! Double-taps \a item at its center. QuickTest's mouse sequences reach a
    //! TapHandler as two single taps and never as doubleTapped, so a touch
    //! double-tap stands in for the double-click.
    function doubleTap(item) {
        const x = item.width / 2
        const y = item.height / 2
        const touch = touchEvent(item)
        touch.press(0, item, x, y).commit()
        touch.release(0, item, x, y).commit()
        touch.press(0, item, x, y).commit()
        touch.release(0, item, x, y).commit()
    }

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
