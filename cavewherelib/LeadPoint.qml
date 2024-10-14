pragma ComponentBehavior: Bound

import QtQuick as QQ
import cavewherelib
import QtQuick.Layouts

PointItem {
    id: pointId
    property Scrap scrap;

    z: selected ? 1 : 0

    onSelectedChanged: {
        if(parent != null) {
            parent.z = selected ? 1 : 0
        }
    }

    LeadListener {
        id: lead
        index: pointId.pointIndex
        scrap: pointId.scrap
    }

    QQ.Image {
        id: questionImage
        source: "qrc:/icons/question.png"
        anchors.centerIn: parent

        visible: !lead.completed || pointId.selected
        opacity: lead.completed ? 0.6 : 1.0

        QQ.MouseArea {
            anchors.fill: parent
            onClicked: pointId.selected = !pointId.selected
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


                ColumnLayout {
                    id: columnLayout

                    CWButton {
                        Layout.alignment: Qt.AlignRight
                        iconSource: "qrc:/icons/x.png"
                        onClicked: pointId.selected = false
                    }

                    SizeEditor {
                        readOnly: true
                        backgroundColor: "#C6C6C6"
                        widthText: lead.width
                        heightText: lead.height
                    }

                    CheckBox {
                        id: checkBox
                        text: "Completed"
                        checked: lead.completed
                        onCheckedChanged: lead.completed = checked

                        QQ.Connections {
                            target: lead
                            function onCompletedChanged() { checkBox.checked = lead.completed }
                        }
                    }

                    Text {
                        visible: text.length > 0
                        text: lead.description
                    }

                    LinkText {
                        text: "Notes"
                        onClicked: linkGenerator.gotoScrap(pointId.scrap)


                        LinkGenerator {
                            id: linkGenerator
                            pageSelectionModel: RootData.pageSelectionModel
                        }
                    }
                }
            }
        }
    }

    QQ.Loader {
        sourceComponent: pointId.selected ? quoteBoxComponent : null
        asynchronous: true
    }
}

