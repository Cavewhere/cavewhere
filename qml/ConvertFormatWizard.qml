import QtQuick 2.0
import QtQuick.Window 2.2
import QtQuick.Dialogs 1.2
import QtQuick.Controls 1.0
import QtQuick.Layouts 1.0
import Cavewhere 1.0

/**
  This wizard (a dialog) that the allows the user to find note images on disk
  for the new format, or create a new folder with all the images in the database.
  */
Window {
    id: window
    width: 550
    height: 500
    modality: Qt.ApplicationModal
    flags: Qt.Dialog
    title: "Convert from Version 1 to Version 2"

    onVisibleChanged:  {
        if(visible) {
            //Updates the images in the wizard
            converter.queryDatabaseImages()
        }
    }

    FileDialog {
        id: findImageDirDialog
        selectFolder: true
        onAccepted: {
            converter.addSearchPath(fileUrl);
        }
    }

    FileDialog {
        id: saveImageToDirectory
        selectFolder: true
        onAccepted: {
            converter.saveImages(fileUrl)
        }
    }

    FormatConverterModel {
        id: converter
        project: rootData.project
        region: rootData.region
    }

    SortFilterProxyModel {
        id: filterProxyModel
        source: converter
        filterRole: "statusRole"
        filterString: showResolvedCheckbox.checked ? "" : "0"
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 10

        Text {
            text: "Cavewhere needs <b>your help to convert to a new format</b>. You will not loose any data."
            Layout.fillWidth: true
            wrapMode: Text.WordWrap
        }

        Text {
            text: "The new format doesn't store images directly (the old format does). \
You need to help Cavewhere locate the images or save the images to disk."
            Layout.fillWidth: true
            wrapMode: Text.WordWrap
        }

        GroupBox {
            Layout.fillWidth: true

            ColumnLayout {
                Text {
                    text: "You have two options:"
                    font.bold: true
                }

                Button {
                    text: "Find images in directories"
                    onPressedChanged: {
                        findImageDirDialog.open()
                    }
                }

                Button {
                    text: "Save images to a directory"
                    onPressedChanged: {
                        saveImageToDirectory.open();
                    }
                }

            }
        }

        RowLayout {
            Layout.fillWidth: true
            CheckBox {
                id: showResolvedCheckbox
                text: "Show resolved images"
                checked: true
            }

            Item {
                width: 10; height: 10
                Layout.fillWidth: true
            }


        }

        TableView {
            id: images
            Layout.fillWidth: true
            Layout.fillHeight: true
            model: filterProxyModel

            //Found, Trip, Image Name, Resolution

            itemDelegate: Loader {
                sourceComponent: {
                    switch(styleData.column) {
                    case 0:
                        return statusComponent
                    case 3:
                        return imageNumberComponent
                    case 4:
                        return pathComponent
                    default:
                        return textComponent
                    }
                }

                Binding {
                    target: item
                    property: "value"
                    value: styleData.value
                }

//                item.text: styleData.value
            }

            TableViewColumn {
                role: "statusRole"
                title: "Status"
                width: 50
            }

            TableViewColumn {
                role: "caveNameRole"
                title: "Cave"
                width: 100
            }

            TableViewColumn {
                role: "tripNameRole"
                title: "Trip"
                width: 100
            }

            TableViewColumn {
                role: "imageRole"
                title: "Image"
                width: 50
            }

            TableViewColumn {
                role: "pathRole"
                title: "Path"
                width: 1000
            }
        }

        Component {
            id: statusComponent
            Image {
                property var value
                fillMode: Image.Pad
                source: {
                    switch(value) {
                        case FormatConverterModel.NeedsResolution:
                            return "qrc:icons/error.png";
                        default:
                            return "qrc:icons/good.png";
                    }
                }
            }
        }

        Component {
            id: imageNumberComponent

            LinkText {
                property var value
                text: "show"
                onClicked: {
                    previewImage.imageId = value
                    previewImage.visible = true;
                }
            }
        }

        Component {
            id: textComponent
            Text {
                id: textId
                property alias value: textId.text
            }
        }

        Component {
            id: pathComponent
            Text {
                id: pathId
                property string value
                property bool empty: value == ""
                text: {
                    if(empty) {
                        return "Empty Path, find or save image"
                    }
                    return value
                }
                color: empty ? "red" : "black"
                font.bold: empty
            }
        }

        RowLayout {
            Layout.alignment: Qt.AlignRight

            Text {
                id: needActionTextId

                property int numberNeedAction: converter.numberOfImages - converter.numberOfResolvedImages;

                text: {
                    if(numberNeedAction != 0) {
                        return (converter.numberOfImages - converter.numberOfResolvedImages) + " images need action"
                    }
                    return "Ready to save";
                }

                font.bold: true
            }

            Button {
                enabled: needActionTextId.numberNeedAction == 0
                text: "Save"
            }
        }
    }

    Rectangle {
        id: previewImage
        property int imageId: 0

        anchors.fill: parent
        visible: false

        MouseArea {
            anchors.fill: parent
        }

        Image {
            anchors.fill: parent
            fillMode: Image.PreserveAspectFit
            source: previewImage.imageId !== 0 ?
                        "image://sqlimagequery/" + previewImage.imageId :
                        ""

            Button {
                anchors.top: parent.top
                anchors.left: parent.left
                anchors.margins: 10
                text: "Back"
                onClicked: {
                    previewImage.visible = false;
                }
            }
        }
    }
}

