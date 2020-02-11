import QtQuick 2.0
import QtQuick.Window 2.0
import QtQuick.Controls 1.0
import QtQuick.Layouts 1.0

Window {
    id: window
    width: 500
    height: 600
    visible: !license.hasReadLicenseAgreement
    color: "#E8E8E8"
    title: "License Agreement"

    modality: Qt.WindowModal

    minimumWidth: logoId.width + 100
    minimumHeight: 300

    ColumnLayout {

        anchors.fill: parent
        anchors.margins: 20

        CavewhereLogo {
            id: logoId
            anchors.horizontalCenter: parent.horizontalCenter
        }

        TextArea {
            Layout.fillWidth: true
            Layout.fillHeight: true
            text: license.text
            readOnly: true
        }

        RowLayout {
            anchors.right: parent.right

            Button {
                text: "Close CaveWhere"
                onClicked: {
                    license.hasReadLicenseAgreement = false
                    Qt.quit()
                }
            }

            Button {
                text: "Accept"
                onClicked: {
                    license.hasReadLicenseAgreement = true
                    window.close()
                }
            }
        }
    }
}
