pragma ComponentBehavior: Bound

import QtQuick as QQ
import QtQuick.Controls as QC
import cavewherelib
import QtQuick.Layouts

QQ.Item {
    id: pointId
    objectName: "leadPoint" + scrapId + "_" + pointIndex

    property Scrap scrap;
    property int pointIndex; //The index in the item list
    property int scrapId; //enable the testcase to work correctly
    property bool selected: false
    property QQ.vector3d position3D;

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

        QQ.TapHandler {
            gesturePolicy: QQ.TapHandler.ReleaseWithinBounds
            onTapped: pointId.selected = !pointId.selected
        }
    }

    QQ.Component {
        id: quoteBoxComponent
        QuoteBox {
            id: quoteBoxId
            objectName: "leadQuoteBox"
            pointAtObject: questionImage
            pointAtObjectPosition: Qt.point(Math.floor(questionImage.width * .5),
                                            questionImage.height)

            QQ.Item {
                width: columnLayout.width
                height: columnLayout.height

                ColumnLayout {
                    id: columnLayout

                    SizeEditor {
                        readOnly: true
                        backgroundColor: "#C6C6C6"
                        widthText: lead.width
                        heightText: lead.height
                    }

                    //This is too easy for the user to mark a lead as completed
                    QC.CheckBox {
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
                        objectName: "description"
                        visible: text.length > 0
                        text: lead.description
                    }

                    LinkText {
                        objectName: "gotoNotes"
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

