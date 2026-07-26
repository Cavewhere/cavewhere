pragma ComponentBehavior: Bound

import QtQuick
import cavewherelib

DoubleClickTextInput {
    id: stationName

    required property PointItem pointItem

    //The scope whose solved stations autocomplete this field. Null leaves the
    //completer inert (a plain text field).
    property Trip trip

    style: Text.Outline
    styleColor: Theme.textInverse
    color: Theme.text
    font.bold: true

    //So we don't add new station when we click on the station
    acceptMousePress: true

    //The scope the editor autocompletes against. Read off the field by
    //StationNameEditor, so every station field shares one editor component.
    readonly property alias scopeModel: scopeStationModel

    //Station entry gets the autocompleting editor
    editorComponent: EditorComponents.stationName

    ScopeStationListModel {
        id: scopeStationModel
        trip: stationName.trip
        network: RootData.linePlotManager.regionNetwork
    }

    // anchors.verticalCenter: stationImage.verticalCenter
    // anchors.left: stationImage.right

    // onFinishedEditting: (newText) => {
    //     noteStationId.scrap.setStationData(Scrap.StationName, noteStationId.pointIndex, newText);
    //     text = newText;
    //     noteStationId.forceActiveFocus();
    // }

    onClicked: pointItem.select()
}
