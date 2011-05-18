import Qt 4.7
import Cavewhere 1.0

DataBox {
    id: stationBox
    dataValidator: StationValidator { }

    onDataValueChanged: dataObject.name = dataObject ? dataValue : ""

    onDataObjectChanged: {
        if(dataObject != null) {
            dataValue = dataObject.name;
            dataObject.nameChanged.connect(updateView);
            //dataObject.reset.connect(updateView);
        }
    }

    function updateView() {
        dataObject ? dataValue = dataObject.name : ""
    }
}
