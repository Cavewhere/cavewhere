import Qt 4.7

DistanceDataBox {
    onDataValueChanged: dataObject ? dataObject.Left = dataValue : ""

    onDataObjectChanged: {
        if(dataObject != null) {
            dataValue = dataObject.Left;
            dataObject.LeftChanged.connect(updateView);
        }
    }

    function updateView() {
        dataObject ? dataValue = dataObject.Left : ""
    }
}
