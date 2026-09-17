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
            objectName: "settingsTabBar"

            Layout.fillHeight: true
            Layout.maximumWidth: 200
            implicitWidth: 200

            // Sketch is still in development; its tab is hidden for this release (#587).
            // model: ["Jobs", "Warping", "PDF / SVG", "Git", "Appearance", "Rendering", "Sketch", "Units"]
            model: ["Jobs", "Warping", "PDF / SVG", "Git", "Appearance", "Rendering", "Units"]
        }

        QC.ScrollView {
            Layout.fillHeight: true
            Layout.fillWidth: true

            StackLayout {
                currentIndex: tabBarId.currentIndex


                JobSettingsItem {
                    objectName: "jobSettingsItem"
                }

                WarpingSettingsItem {
                    objectName: "warpingSettingsItem"
                    warpingSettings: RootData.scrapManager.warpingSettings
                }

                // RenderingSettingsItem {

                // }

                PDFSettingsItem {
                    objectName: "pdfSettingsItem"
                }

                GitSettingsItem {

                }

                AppearanceSettingsItem {
                    objectName: "appearanceSettingsItem"
                }

                RenderingSettingsItem {
                    objectName: "renderingSettingsItem"
                }

                // SketchSettingsItem {

                // }

                UnitsSettingsItem {

                }
            }
        }
    }
}
