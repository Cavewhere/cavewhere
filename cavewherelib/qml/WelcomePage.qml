import QtQuick as QQ
import QtQuick.Shapes
import QtQuick.Controls as QC
import QtQuick.Layouts
import cavewherelib
import QQuickGit

QC.ScrollView {
    id: sketchPageId

    required property Person account
    // required property string

    function update() {
        tempPerson.name = account.name
        tempPerson.email = account.email
    }

    ColumnLayout {
        id: columnLayoutId

        // anchors.fill: parent
        // anchors.margins: 10
        x: Math.max(0.0, sketchPageId.width / 2.0 - width / 2.0)
        width: Math.min(sketchPageId.width - 40, 500)


        QC.Label {
            text: "Let's set you up!"
            Layout.alignment: Qt.AlignHCenter
            font.pixelSize: Theme.fontSizeXLarge
            font.bold: true
        }

        QQ.Item {
            implicitHeight: versionControlId.height

            BodyText {
                id: versionControlId
                width: columnLayoutId.width
                text: "CaveWhere uses version control (Git) to keep track of changes to your cave data. Your name and email are used to record who made each change—just like signing your work. This information stays with your files and helps with collaboration, even if you're working offline."
                wrapMode: QC.Label.WordWrap
            }
        }

        QQ.Item {
            implicitHeight: versionControlPrivacyId.height

            BodyText {
                id: versionControlPrivacyId
                width: columnLayoutId.width
                font.italic: true
                font.pixelSize: versionControlId.font.pixelSize * 0.95
                text: "This information is only used within your version history and is not shared online unless you choose to sync with a service like GitHub."
                wrapMode: QC.Label.WordWrap
            }
        }

        Spacer {}

        PersonEdit {
            id: personEdit
            implicitWidth: columnLayoutId.width * 0.5
            nextTab: nextButtonId.button
            account: Person {
                id: tempPerson
            }
        }

        ButtonShake {
            id: nextButtonId
            Layout.alignment: Qt.AlignRight
            button.objectName: "nextButton"
            button.text: "Next"
            button.onClicked: {
                if(tempPerson.isValid) {
                    sketchPageId.account.name = tempPerson.name
                    sketchPageId.account.email = tempPerson.email
                } else {
                    personEdit.showErrors = true
                    shake();
                }
            }
        }

        QQ.Item {
            Layout.fillHeight: true
        }
    }

    QQ.Component.onCompleted: {
        update();
    }
}
