import Qt 4.7

DistanceDataBox {
    onDataValueChanged: dataObject ? dataObject.Up = dataValue : ""

    function updateView() {
        dataObject ? dataValue = dataObject.Up : ""
    }
}
