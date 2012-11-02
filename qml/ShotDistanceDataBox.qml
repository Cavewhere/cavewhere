import QtQuick 2.0
import QtDesktop 0.2 as Desktop
import Cavewhere 1.0

DataBox {
    id: dataBoxId

    property bool distanceIncluded: true //This show's if the distance is include (true) or excluded

    Rectangle {
        visible: !distanceIncluded

        anchors.bottom: parent.bottom
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottomMargin: 2

        width: textId.width + 4
        height: textId.height - 2

        color: "gray"
        Text {
            id: textId

            anchors.centerIn: parent
            anchors.verticalCenterOffset: 1

            font.family: "monospace"
            font.pixelSize: 10
            font.bold: true
            color: "#EEEEEE"
            text: "Excluded"
        }
    }

    ContextMenuButton {
        id: menuButtonId
        visible: dataBoxId.focus

        anchors.right: parent.right
        anchors.top: parent.top
        anchors.margins: 3

        opacity: state == "" ? .75 : 1.0

        MenuItem {
            text: distanceIncluded ? "Exclude Distance" : "Include Distance"
            onTriggered: {
                surveyChunk.setData(SurveyChunk.ShotDistanceIncludedRole, rowIndex, !distanceIncluded)
            }
        }
    }
}
