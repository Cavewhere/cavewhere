import QtQuick 2.0
import cavewherelib

DataBox {
    id: stationDistanceBox
    dataValidator: DistanceValidator {}

    StationMenu {
        id: removeMenuId
        model: stationDistanceBox.model
        dataValue: stationDistanceBox.dataValue
        removePreview: stationDistanceBox.removePreview
    }

    rightClickMenuLoader: removeMenuId
}
