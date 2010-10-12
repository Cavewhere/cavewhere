import Qt 4.7

DistanceDataBox {
    onDataValueChanged: dataObject ? dataObject.Distance = dataValue : ""

    function updateView() {
        dataObject ? dataValue = dataObject.Distance : ""
    }
}
