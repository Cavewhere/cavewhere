import Qt 4.7

ClinoReadBox {
    readingText: "bs"

    onDataValueChanged: dataObject ? dataObject.BackClino = dataValue : ""

    function updateView() {
        dataObject ? dataValue = dataObject.BackClino : ""
    }
}
