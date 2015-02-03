import QtQuick 2.0
import Cavewhere 1.0
import QtQuick.Controls 1.2
import QtQuick.Layouts 1.1

PointItem {
    id: point
    property Scrap scrap;

    LeadListener {
        id: lead
        index: pointIndex
        scrap: point.scrap
    }

    Image {
        id: questionImage
        source: "qrc:/icons/question.png"
        anchors.centerIn: parent

        visible: !lead.completed || selected
        opacity: lead.completed ? 0.6 : 1.0

        MouseArea {
            anchors.fill: parent
            onClicked: selected = !selected
        }
    }

    Component {
        id: quoteBoxComponent
        QuoteBox {
            pointAtObject: questionImage
            pointAtObjectPosition: Qt.point(Math.floor(questionImage.width * .5),
                                            questionImage.height)

            Item {
                width: columnLayout.width
                height: columnLayout.height

                CWButton {
                    anchors.right: parent.right
                    iconSource: "qrc:/icons/x.png"
                    onClicked: selected = false
                }

                ColumnLayout {
                    id: columnLayout

                    CheckBox {
                        id: checkBox
                        text: "Completed"
                        checked: lead.completed
                        onCheckedChanged: lead.completed = checked

                        Connections {
                            target: lead
                            onCompletedChanged: checkBox.checked = lead.completed
                        }
                    }

                    SizeEditor {
                        readOnly: true
                        backgroundColor: "#C6C6C6"
                        widthText: lead.width
                        heightText: lead.height
                    }

                    Text {
                        visible: text.length > 0
                        text: lead.description
                    }

                    LinkText {
                        text: "Notes"
                        onClicked: linkGenerator.gotoScrap(scrap)


                        LinkGenerator {
                            id: linkGenerator
                            pageSelectionModel: rootData.pageSelectionModel
                        }
                    }
                }
            }
        }
    }

    Loader {
        sourceComponent: selected ? quoteBoxComponent : null
        asynchronous: true
    }
}

