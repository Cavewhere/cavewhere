import QtQuick as QQ
import QtQuick.Layouts
import QtQuick.Controls as QC
import cavewherelib

ColumnLayout {
    id: itemId
    property TurnTableInteraction turnTableInteraction;

    implicitWidth: rowLayout.width

    RowLayout {
        id: rowLayout

        InformationButton {
            showItemOnClick: helpAreaId
        }

        ClickTextInput {
            text: Number(itemId.turnTableInteraction.pitch).toFixed(1)
            validator: doubleValidatorId
            enabled: !itemId.turnTableInteraction.pitchLocked
            onFinishedEditting: (newText) => {
                pitchAnimation.to = newText
                pitchAnimation.restart()
            }
        }

        Text {
            text: "°"
        }

        ClinoValidator {
            id: doubleValidatorId
        }

        ColumnLayout {
            enabled: !itemId.turnTableInteraction.pitchLocked

            QC.Button {
                id: planButtonId
                text: "Plan"
                implicitWidth: profileButtonId.width
                onClicked: {
                    pitchAnimation.to = 90
                    pitchAnimation.restart()
                }
                enabled: itemId.turnTableInteraction.pitch !== 90.0
            }

            QC.Button {
                id: profileButtonId
                objectName: "profileButton"
                text: "Profile"
                onClicked: {
                    pitchAnimation.to = 0.0
                    pitchAnimation.restart()
                }
                enabled: itemId.turnTableInteraction.pitch !== 0.0
            }
        }

        LockButton {
            down: itemId.turnTableInteraction.pitchLocked
            onClicked: {
                itemId.turnTableInteraction.pitchLocked = !itemId.turnTableInteraction.pitchLocked
            }
        }
    }

    QQ.NumberAnimation {
        id: pitchAnimation
        target: itemId.turnTableInteraction;
        property: "pitch";
        duration: 200;
        easing.type: QQ.Easing.InOutQuad
    }

    HelpArea {
        id: helpAreaId
        Layout.fillWidth: true
        text: "The vertical angle is the (in degrees between -90.0 and 90.0) clinometer direction that the view is facing." +
"<br><br>Clicking on plan, will cause the view to look down at a 90.0°." +
"<br>Clicking on profile, will cause the view to look at 0.0°."
    }
}
