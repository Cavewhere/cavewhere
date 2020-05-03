import QtQuick 2.0 as QQ
import QtQuick.Layouts 1.1
import Cavewhere 1.0 as Cavewhere
import QtQuick.Controls 2.0

ColumnLayout {
    property TurnTableInteraction turnTableInteraction;

    RowLayout {
        id: rowLayout
        Layout.fillWidth: true

        InformationButton {
            showItemOnClick: helpAreaId
        }

        ClickTextInput {
            text: Number(turnTableInteraction.pitch).toFixed(1)
            validator: doubleValidatorId
            onFinishedEditting: {
                pitchAnimation.to = newText
                pitchAnimation.restart()
            }
        }

        Text {
            text: "°"
        }

        LockButton {
            down: turnTableInteraction.pitchLocked
            onClicked: {
                turnTableInteraction.pitchLocked = !turnTableInteraction.pitchLocked
            }
        }

        Cavewhere.ClinoValidator {
            id: doubleValidatorId
        }

        ColumnLayout {
            Button {
                id: planButtonId
                text: "Plan"
                width: profileButtonId.width
                onClicked: {
                    pitchAnimation.to = 90
                    pitchAnimation.restart()
                }
                enabled: turnTableInteraction.pitch !== 90.0
            }

            Button {
                id: profileButtonId
                text: "Profile"
                onClicked: {
                    pitchAnimation.to = 0.0
                    pitchAnimation.restart()
                }
                enabled: turnTableInteraction.pitch !== 0.0
            }
        }
    }

    QQ.NumberAnimation {
        id: pitchAnimation
        target: turnTableInteraction;
        property: "pitch";
        duration: 200;
        easing.type: QQ.Easing.InOutQuad
    }

    HelpArea {
        id: helpAreaId
        Layout.fillWidth: true
        text: "The vertical angle is the (in degrees between -90.0 and 90.0) clinometer direction that the view is facing.
<br><br>Clicking on plan, will cause the view to look down at a 90.0°.
<br>Clicking on profile, will cause the view to look at 0.0°."
    }
}
