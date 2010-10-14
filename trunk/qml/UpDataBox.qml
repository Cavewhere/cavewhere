import Qt 4.7

DistanceDataBox {
    onDataValueChanged: dataObject ? dataObject.Up = dataValue : ""

    onDataObjectChanged: {
        if(dataObject != null) {
            dataValue = dataObject.Up;
            dataObject.UpChanged.connect(updateView);
        }
    }

    function updateView() {
        dataObject ? dataValue = dataObject.Up : ""
    }
}
