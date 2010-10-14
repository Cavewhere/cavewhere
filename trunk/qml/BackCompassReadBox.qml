import Qt 4.7

CompassReadBox {
    readingText: "bs"

    onDataValueChanged: dataObject ? dataObject.BackCompass = dataValue : ""

    onDataObjectChanged: {
        if(dataObject != null) {
            dataValue = dataObject.BackCompass;
            dataObject.BackCompassChanged.connect(updateView);
        }
    }

    function updateView() {
        dataObject ? dataValue = dataObject.BackCompass : ""
    }
}
