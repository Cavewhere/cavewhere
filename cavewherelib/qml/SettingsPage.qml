import QtQuick.Controls as QC
import QtQuick.Layouts
import cavewherelib

StandardPage {
    id: pageId

    RowLayout {
        anchors.fill: parent
        anchors.margins: Theme.pageMargin

        TabViewVertical {
            id: tabBarId

            Layout.fillHeight: true
            Layout.maximumWidth: 200
            implicitWidth: 200

            model: ["Jobs", "Warping", "PDF / SVG", "Git", "Appearance", "Rendering", "Sketch"]
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

                GitSettingsItem {

                }

                AppearanceSettingsItem {

                }

                RenderingSettingsItem {

                }

                SketchSettingsItem {

                }
            }
        }
    }
}
