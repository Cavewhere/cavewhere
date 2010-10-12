import Qt 4.7

DataBox {
    id: stationBox
    dataValidator: RegExpValidator {
        regExp: /\w+(\.\w+)*/;
    }

    onDataValueChanged: dataObject ? dataObject.Name = dataValue : ""

    function updateView() {
        dataObject ? dataValue = dataObject.Name : ""
    }
}
