import QtQuick 2.0

DistanceDataBox {
    onDataValueChanged: dataObject ? dataObject.Distance = dataValue : ""

    onDataObjectChanged: {
        if(dataObject != null) {
            dataValue = dataObject.Distance;
            dataObject.DistanceChanged.connect(updateView);
        }
    }


    function updateView() {
        dataObject ? dataValue = dataObject.Distance : ""
    }
}
