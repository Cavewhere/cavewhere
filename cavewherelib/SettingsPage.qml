import QtQuick.Controls 2.12 as QC
import QtQuick.Layouts 1.12
import cavewherelib

StandardPage {
    id: pageId

    RowLayout {
        anchors.fill: parent

        TabViewVertical {
            id: tabBarId

            Layout.fillHeight: true
            Layout.maximumWidth: 200
            implicitWidth: 200

            model: ["Jobs", "Rendering", "PDF"]
        }

        QC.ScrollView {
            Layout.fillHeight: true
            Layout.fillWidth: true

            StackLayout {
                currentIndex: tabBarId.currentIndex


                JobSettingsItem {

                }

                RenderingSettingsItem {

                }

                PDFSettingsItem {

                }
            }
        }
    }
}
