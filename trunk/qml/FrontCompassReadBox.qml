import Qt 4.7

CompassReadBox {
    readingText: "fs"

    onDataValueChanged: dataObject ? dataObject.Compass = dataValue : ""

    function updateView() {
        dataObject ? dataValue = dataObject.Compass : ""
    }
}
