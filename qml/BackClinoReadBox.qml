import Qt 4.7

ClinoReadBox {
    readingText: "bs"

    onDataValueChanged: dataObject ? dataObject.BackClino = dataValue : ""

    onDataObjectChanged: {
        if(dataObject != null) {
            dataValue = dataObject.BackClino;
            dataObject.BackClinoChanged.connect(updateView);
        }
    }

    function updateView() {
        dataObject ? dataValue = dataObject.BackClino : ""
    }
}
