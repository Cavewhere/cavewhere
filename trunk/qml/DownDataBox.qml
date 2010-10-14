import Qt 4.7

DistanceDataBox {
    onDataValueChanged: dataObject ? dataObject.Down = dataValue : ""

    onDataObjectChanged: {
        if(dataObject != null) {
            dataValue = dataObject.Down;
            dataObject.DownChanged.connect(updateView);
        }
    }

    function updateView() {
        dataObject ? dataValue = dataObject.Down : ""
    }
}
