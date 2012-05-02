// import QtQuick 1.0 // to target S60 5th Edition or Maemo 5
import QtQuick 2.0
//import QtDesktop 0.2 as Desktop
import Cavewhere 1.0

Item {
    id: iconBar


    Row {

        anchors.left: parent.left
        anchors.leftMargin: 5
        anchors.verticalCenter: parent.verticalCenter

        spacing: 3

        Button {
            id: exportButton
            text: "Export"

            onClicked: {
                var globalPoint = mapToItem(null, 0, exportButton.height);
                exportMenuWindow.visible = true
                exportMenuWindow.x = globalPoint.x
                exportMenuWindow.y = globalPoint.y
            }


            ContextMenu {
                id: exportMenuWindow

                Menu {
                    text: "Survex"

                    MenuItem {
                        text: "Current Trip"
                    }

                    MenuItem {
                        text: "Current Cave"
                    }

                }

                Menu {
                    text: "Compass"

                    MenuItem {
                        text: "Current Trip Compass"
                    }

                    MenuItem {
                        text: "Current Cave Compass"
                    }
                }

                Menu {
                    text: "Test"

                    MenuItem {
                        text: "Sauce !Current Trip Compass"
                    }

                    MenuItem {
                        text: "Moo Current Cave Compass"
                    }

                    Menu {
                        text: "Touch me"

                        MenuItem {
                            text: "Wahoo!"
                        }

                        MenuItem {
                            text: "Sauce"
                        }

                        Menu {
                            text: "Menu"

                            Menu {
                                text: "Menu2"

                                MenuItem {

                                    width: menuImage.width
                                    height: menuImage.height

                                    Image {
                                        id: menuImage
                                        source: "/Users/vpicaver/Documents/Caving\ Data/China/2012/Drafting/Quankou/scanGref/wetdreams2_gref.jpg"
                                    }
                                }
                            }
                        }
                    }
                }

//                onXChanged: {
//                    console.log("X change: " + x)
//                }

            }

            // FIXME: Fix export menu
//            Desktop.ContextMenu {
//                id: exportContextMenu

//                Desktop.Menu {
//                    text: "Survex"

//                    ExportSurveyMenuItem {
//                        prefixText: "Current trip"
//                        currentText: surveyExportManager.currentTripName
//                        onTriggered: surveyExportManager.openExportSurvexTripFileDialog()
//                    }

//                    ExportSurveyMenuItem {
//                        prefixText: "Current cave"
//                        currentText: surveyExportManager.currentCaveName
//                        onTriggered: surveyExportManager.openExportSurvexCaveFileDialog()
//                    }

//                    Desktop.MenuItem {
//                        text: "Region (all caves)"
//                        onTriggered: surveyExportManager.openExportSurvexRegionFileDialog()
//                    }
//                }

//                Desktop.Menu {
//                    text: "Compass"

//                    ExportSurveyMenuItem {
//                        prefixText: "Current cave"
//                        currentText: surveyExportManager.currentCaveName
//                        onTriggered: surveyExportManager.openExportCompassCaveFileDialog()
//                    }
//                }
//            }
        }


        Button {
            id: importButton

            text: "Import"

            onClicked: {
                var globalPoint = mapToItem(null, 0, 2 * importButton.height);
                importContextMenu.showPopup(globalPoint.x, globalPoint.y)
            }

            // FIXME: Fix import menu
//            Desktop.ContextMenu {
//                id: importContextMenu

//                Desktop.MenuItem {
//                    text: "Survex (.svx)"
//                    onTriggered: surveyImportManager.importSurvex()
//                }

//            }
        }
    }



}
