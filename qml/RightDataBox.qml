import Qt 4.7

DistanceDataBox {
    onDataValueChanged: dataObject ? dataObject.right = dataValue : ""

    onDataObjectChanged: {
        if(dataObject != null) {
            dataValue = dataObject.right;
            dataObject.rightChanged.connect(updateView);
            dataObject.reset.connect(updateView);
        }
    }

    function updateView() {
        dataObject ? dataValue = dataObject.right : ""
    }
}
