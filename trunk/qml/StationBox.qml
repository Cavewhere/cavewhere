import Qt 4.7

DataBox {
    id: stationBox
    dataValidator: RegExpValidator {
        regExp: /\w+(\.\w+)*/;
    }

    onDataValueChanged: dataObject.Name = dataObject ? dataValue : ""

    onDataObjectChanged: {
        if(dataObject != null) {
            dataValue = dataObject.Name;
            dataObject.NameChanged.connect(updateView);
        }
    }

    function updateView() {
        dataObject ? dataValue = dataObject.Name : ""
    }
}
