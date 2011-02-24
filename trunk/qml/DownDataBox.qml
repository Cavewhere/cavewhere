import Qt 4.7

DistanceDataBox {
    onDataValueChanged: dataObject ? dataObject.down = dataValue : ""

    onDataObjectChanged: {
        if(dataObject != null) {
            dataValue = dataObject.down;
            dataObject.downChanged.connect(updateView);
            dataObject.reset.connect(updateView);
        }
    }

    function updateView() {
        dataObject ? dataValue = dataObject.down : ""
    }
}
