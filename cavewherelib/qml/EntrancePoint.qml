pragma ComponentBehavior: Bound

import QtQuick as QQ
import cavewherelib

// The 2D stand-in for one entrance marker. The pin and its text draw as
// billboards of the EntranceView's keyed layers; cwEntranceView reads this
// item's marker visibility for both and places the item at the fix's solved
// position. The popup arrives with the tap handling.
PositionItem {
    id: pointId
    objectName: "entrancePoint_" + stationName

    property SurveyNode node
    property string stationName
    property string fixId
    required property SelectionManager selectionManager //Injected by cwEntranceView
    required property EntranceView entranceView //Injected by cwEntranceView

    property alias marker: hitTarget

    // Tap target sized to the pin and standing on the station, tip down, so the
    // hit area matches the drawn sprite.
    QQ.Item {
        id: hitTarget

        width: pointId.entranceView.spriteSize.width
        height: pointId.entranceView.spriteSize.height
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: parent.verticalCenter
    }
}
