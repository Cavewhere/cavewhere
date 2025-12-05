import QtQuick
import QtQuick.Controls as QC
import cavewherelib
QC.SpinBox {
    id: spinBox
    from: 0
    value: decimalToInt(realValue)
    to: decimalToInt(100)
    stepSize: decimalToInt(realStepSize)
    editable: true

    property int decimals: 2
    property real realValue: 0.0
    property real realStepSize: 0.1
    readonly property int decimalFactor: Math.pow(10, decimals)

    function decimalToInt(decimal) {
        return decimal * decimalFactor
    }

    function valueToDecimal() {
        return value / decimalFactor;
    }

    onValueModified: {
        realValue = valueToDecimal()
    }

    validator: DoubleValidator {
        bottom: Math.min(spinBox.from, spinBox.to)
        top:  Math.max(spinBox.from, spinBox.to)
        decimals: spinBox.decimals
        notation: DoubleValidator.StandardNotation
    }

    textFromValue: function(value, locale) {
        return Number(valueToDecimal()).toLocaleString(locale, 'f', spinBox.decimals)
    }

    valueFromText: function(text, locale) {
        return Math.round(Number.fromLocaleString(locale, text) * decimalFactor)
    }
}
