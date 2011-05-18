import Qt 4.7

CompassReadBox {
    readingText: "fs"

    onDataValueChanged: dataObject ? dataObject.Compass = dataValue : ""

    onDataObjectChanged: {
        if(dataObject != null) {
            dataValue = dataObject.Compass;
            dataObject.CompassChanged.connect(updateView);
        }
    }

    function updateView() {
        dataObject ? dataValue = dataObject.Compass : ""
    }
}
