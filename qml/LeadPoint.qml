import QtQuick 2.12 as QQ
import Cavewhere 1.0
import QtQuick.Layouts 1.1

PointItem {
    id: point
    property Scrap scrap;

    z: selected ? 1 : 0

    onSelectedChanged: {
        if(parent != null) {
            parent.z = selected ? 1 : 0
        }
    }

    LeadListener {
        id: lead
        index: pointIndex
        scrap: point.scrap
    }

    QQ.Image {
        id: questionImage
        source: "qrc:/icons/question.png"
        anchors.centerIn: parent

        visible: !lead.completed || selected
        opacity: lead.completed ? 0.6 : 1.0

        QQ.MouseArea {
            anchors.fill: parent
            onClicked: selected = !selected
        }
    }

    QQ.Component {
        id: quoteBoxComponent
        QuoteBox {
            pointAtObject: questionImage
            pointAtObjectPosition: Qt.point(Math.floor(questionImage.width * .5),
                                            questionImage.height)

            QQ.Item {
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

                        QQ.Connections {
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

    QQ.Loader {
        sourceComponent: selected ? quoteBoxComponent : null
        asynchronous: true
    }
}

