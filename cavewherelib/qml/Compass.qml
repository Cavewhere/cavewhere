import QtQuick
import cavewherelib

CompassBackendItem {
    id: compass

    component CenteredLabel: Item {
        property alias text: label.text
        Label3d {
            id: label
            anchors.centerIn: parent
        }
    }

    CenteredLabel {
        text: "N"
        x: compass.labelNorthPosition.x
        y: compass.labelNorthPosition.y
    }

    CenteredLabel {
        text: "S"
        x: compass.labelSouthPosition.x
        y: compass.labelSouthPosition.y
    }

    CenteredLabel {
        text: "E"
        x: compass.labelEastPosition.x
        y: compass.labelEastPosition.y
    }

    CenteredLabel {
        text: "W"
        x: compass.labelWestPosition.x
        y: compass.labelWestPosition.y
    }
}
