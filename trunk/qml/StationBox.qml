import Qt 4.7
import Cavewhere 1.0

DataBox {
    id: stationBox
    dataValidator: StationValidator { }

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
