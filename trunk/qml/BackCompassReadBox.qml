import Qt 4.7

CompassReadBox {
    readingText: "bs"

    onDataValueChanged: dataObject ? dataObject.BackCompass = dataValue : ""

    function updateView() {
        dataObject ? dataValue = dataObject.BackCompass : ""
    }
}
