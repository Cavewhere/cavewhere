import Qt 4.7

ClinoReadBox {
    readingText: "fs"

    onDataValueChanged: dataObject ? dataObject.Clino = dataValue : ""

    function updateView() {
        dataObject ? dataValue = dataObject.Clino : ""
    }
}
