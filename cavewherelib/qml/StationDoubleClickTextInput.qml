import QtQuick
import cavewherelib

DoubleClickTextInput {
    id: stationName

    required property PointItem pointItem

    style: Text.Outline
    styleColor: "#FFFFFF"
    font.bold: true

    //So we don't add new station when we click on the station
    acceptMousePress: true

    // anchors.verticalCenter: stationImage.verticalCenter
    // anchors.left: stationImage.right

    // onFinishedEditting: (newText) => {
    //     noteStationId.scrap.setStationData(Scrap.StationName, noteStationId.pointIndex, newText);
    //     text = newText;
    //     noteStationId.forceActiveFocus();
    // }

    onClicked: pointItem.select()
}
