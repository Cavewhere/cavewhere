import Qt 4.7

ClinoReadBox {
    readingText: "fs"

    onDataValueChanged: dataObject ? dataObject.Clino = dataValue : ""

    onDataObjectChanged: {
        if(dataObject != null) {
            dataValue = dataObject.Clino;
            dataObject.ClinoChanged.connect(updateView);
        }
    }

    function updateView() {
        dataObject ? dataValue = dataObject.Clino : ""
    }
}
