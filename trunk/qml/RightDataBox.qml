import Qt 4.7

DistanceDataBox {
    onDataValueChanged: dataObject ? dataObject.Right = dataValue : ""

    function updateView() {
        dataObject ? dataValue = dataObject.Right : ""
    }
}
