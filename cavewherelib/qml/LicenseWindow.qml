import QtQuick.Window
import QtQuick.Controls as QC
import QtQuick.Layouts
import cavewherelib

Window {
    id: window
    width: 500
    height: 600
    visible: !RootData.license.hasReadLicenseAgreement
    color: Theme.surfaceMuted
    title: "License Agreement"

    modality: Qt.WindowModal

    minimumWidth: logoId.width + 100
    minimumHeight: 300

    ColumnLayout {

        anchors.fill: parent
        anchors.margins: 20

        CavewhereLogo {
            id: logoId
            Layout.alignment: Qt.AlignHCenter
        }

        TextArea {
            Layout.fillWidth: true
            Layout.fillHeight: true
            text: RootData.license.text
            readOnly: true
        }

        RowLayout {
            Layout.alignment: Qt.AlignRight

            Button {
                text: "Close CaveWhere"
                onClicked: {
                    RootData.license.hasReadLicenseAgreement = false
                    Qt.quit()
                }
            }

            Button {
                text: "Accept"
                onClicked: {
                    RootData.license.hasReadLicenseAgreement = true
                    window.close()
                }
            }
        }
    }
}
