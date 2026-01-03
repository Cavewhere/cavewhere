import QtQuick.Controls as QC
import QtQuick.Layouts
import cavewherelib

StandardPage {
    id: pageId

    RowLayout {
        anchors.fill: parent
        anchors.margins: 5

        TabViewVertical {
            id: tabBarId

            Layout.fillHeight: true
            Layout.maximumWidth: 200
            implicitWidth: 200

            // model: ["Jobs", "Rendering", "PDF"]
            model: ["Jobs", "Warping", "PDF / SVG"]
        }

        QC.ScrollView {
            Layout.fillHeight: true
            Layout.fillWidth: true

            StackLayout {
                currentIndex: tabBarId.currentIndex


                JobSettingsItem {

                }

                WarpingSettingsItem {
                    warpingSettings: RootData.scrapManager.warpingSettings
                }

                // RenderingSettingsItem {

                // }

                PDFSettingsItem {

                }
            }
        }
    }
}
