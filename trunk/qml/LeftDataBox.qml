import Qt 4.7

DistanceDataBox {
    onDataValueChanged: dataObject ? dataObject.left = dataValue : ""

    onDataObjectChanged: {
        if(dataObject != null) {
            dataValue = dataObject.left;
            dataObject.leftChanged.connect(updateView);
            dataObject.reset.connect(updateView);
        }
    }

    function updateView() {
        dataObject ? dataValue = dataObject.left : ""
    }
}
