import Qt 4.7

DistanceDataBox {
    onDataValueChanged: dataObject ? dataObject.Right = dataValue : ""

    onDataObjectChanged: {
        if(dataObject != null) {
            dataValue = dataObject.Right;
            dataObject.RightChanged.connect(updateView);
        }
    }

    function updateView() {
        dataObject ? dataValue = dataObject.Right : ""
    }
}
