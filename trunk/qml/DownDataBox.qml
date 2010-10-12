import Qt 4.7

DistanceDataBox {
    onDataValueChanged: dataObject ? dataObject.Down = dataValue : ""

    function updateView() {
        dataObject ? dataValue = dataObject.Down : ""
    }
}
