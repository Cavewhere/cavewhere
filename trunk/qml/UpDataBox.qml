import Qt 4.7

DistanceDataBox {
    onDataValueChanged: dataObject ? dataObject.up = dataValue : ""

    onDataObjectChanged: {
        if(dataObject != null) {
            dataValue = dataObject.up;
            dataObject.upChanged.connect(updateView);
            dataObject.reset.connect(updateView);
        }
    }

    function updateView() {
        dataObject ? dataValue = dataObject.up : ""
    }
}
