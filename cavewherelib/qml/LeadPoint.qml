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
    required property SelectionManager selectionManager; //Injected by cwLeadView
    required property LeadView leadView; //Injected by cwLeadView

    // The question mark renders as a depth-occluded billboard; cwLeadView reads
    // this to feed the 3D draw. The QuoteBox popup is a sibling, so it stays a
    // normal 2D overlay rather than being pulled into the occluded texture.
    property alias billboardContent: questionImage

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
    }

    // Separate tap target. questionImage is the billboard's content, which
    // cwQuickItemSubscene pulls out of the 2D scene via refFromEffectItem(hide)
    // — a hidden item receives no input, so the TapHandler must live on an item
    // the billboard does NOT reference. Sized and centred on the marker so the
    // hit area matches the drawn billboard; gated to the marker's visibility so
    // a hidden lead isn't tappable.
    QQ.Item {
        id: hitTarget
        anchors.fill: questionImage

        visible: questionImage.visible

        QQ.TapHandler {
            gesturePolicy: QQ.TapHandler.ReleaseWithinBounds
            onTapped: (eventPoint) => {
                // Route through the manager so selecting this lead deselects any
                // other open lead; tapping the selected lead again closes it.
                // Ignore a tap that lands on a part of the marker hidden behind
                // geometry (the click must not pass through walls); closing is
                // always allowed. isOccluded tests the tapped pixel itself, so it
                // matches the silhouette the billboard is drawn with — map the tap
                // into the leadView's (viewport) coordinate space for the ray.
                if (pointId.selected) {
                    pointId.selectionManager.selectedItem = null
                } else {
                    let tap = hitTarget.mapToItem(pointId.leadView, eventPoint.position)
                    if (!pointId.leadView.isOccluded(pointId.position3D, tap)) {
                        pointId.selectionManager.selectedItem = pointId
                    }
                }
            }
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

                    QC.Label {
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

