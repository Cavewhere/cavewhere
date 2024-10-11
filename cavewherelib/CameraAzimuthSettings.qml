import QtQuick 2.0 as QQ
import QtQuick.Layouts 1.1
import cavewherelib

ColumnLayout {

    property TurnTableInteraction turnTableInteraction

    implicitWidth: gridId.width
//    height: 200

    GridLayout {
        id: gridId
        rows: 3
        columns: 3

        InformationButton {
            showItemOnClick: directionHelpAreaId
        }

        Button {
            id: northButton
            text: "North"
            onClicked: {
                azimuthAnimationId.restartRotation(0.0)
            }
            enabled: turnTableInteraction.azimuth !== 0.0
        }

        QQ.Item { width:1; height:1 }

        Button {
            id: westButton
            text: "West"
            width: northButton.width
            onClicked: {
                azimuthAnimationId.restartRotation(270.0)
            }
            enabled: turnTableInteraction.azimuth !== 270.0
        }

        RowLayout {

            Layout.alignment: Qt.AlignCenter
            width: northButton.width

            ClickTextInput {
                text: Number(turnTableInteraction.azimuth).toFixed(1)
                validator: doubleValidatorId
                onFinishedEditting: {
                    azimuthAnimationId.to = newText
                    azimuthAnimationId.restart()
                }
            }

            CompassValidator {
                id: doubleValidatorId
            }

            Text {
                text: "°"
            }
        }

        Button {
            id: eastButton
            text: "East"
            width: northButton.width
            onClicked: {
                azimuthAnimationId.restartRotation(90.0)
            }
            enabled: turnTableInteraction.azimuth !== 90.0
        }

        QQ.Item { width:1; height:1 }
        Button {
            id: southButton
            text: "South"
            width: northButton.width
            onClicked: {
                azimuthAnimationId.restartRotation(180.0)
            }
            enabled: turnTableInteraction.azimuth !== 180.0
        }
        QQ.Item { width:1; height:1 }

    }

    QQ.NumberAnimation {
        id: azimuthAnimationId
        target: turnTableInteraction;
        property: "azimuth";
        duration: 200;
        easing.type: QQ.Easing.InOutQuad

        function restartRotation(toRotation) {

            if(toRotation > 180) {
                toRotation = toRotation - 360;
            }

            var from = turnTableInteraction.azimuth;
            if(from > 180) {
                from = from - 360.0;
            }

            var angle = Math.abs((toRotation - turnTableInteraction.azimuth) % 360.0);
            if(angle > 180) {
                angle = 360.0 - angle;
            }

            var to = 0
            if(from <= toRotation) {
                //Shortest path is clockwise
                to = from + angle
            } else {
                //Shortest path is counter clockwise
                to = from - angle
            }

            azimuthAnimationId.from = from
            azimuthAnimationId.to = to
            azimuthAnimationId.restart()
        }
    }

    HelpArea {
        id: directionHelpAreaId
        Layout.fillWidth: true
        text: "The views azimuth (in degrees between 0.0 and 360.0) is
             the compass direction that the view is facing.
            <ul><li>0.0° for North<li>90.0° for East<li>180.0° for South<li>270.0° for West</ul>";
    }
}
