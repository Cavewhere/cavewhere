import Qt 4.7

DistanceDataBox {
    onDataValueChanged: dataObject ? dataObject.Left = dataValue : ""

    function updateView() {
        dataObject ? dataValue = dataObject.Left : ""
    }
}
