pragma ComponentBehavior: Bound

import QtQuick as QQ
import QtQuick.Controls as QC
import QtQuick.Layouts
import cavewherelib

PositionItem {
    id: pointId
    objectName: "leadPoint" + scrapId + "_" + pointIndex

    property Scrap scrap;
    property int pointIndex; //The index in the item list
    property int scrapId; //enable the testcase to work correctly
    property bool selected: false
    required property SelectionManager selectionManager; //Injected by cwMarkerView
    required property MarkerView markerView; //Injected by cwMarkerView

    // The marker's stand-in in the 2D tree. The question mark itself draws as one
    // instance of the MarkerView's sprite layer; cwMarkerView reads this item's
    // visibility and opacity for that instance. The QuoteBox popup is a sibling,
    // so it stays a normal 2D overlay.
    property alias marker: hitTarget

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

    // Tap target sized and centered on the drawn marker, so the hit area matches
    // the sprite. cwMarkerView reads these for the lead's sprite instance; a hidden
    // marker is also untappable. Opacity has no 2D effect on an empty item.
    QQ.Item {
        id: hitTarget

        property int spriteIndex: LeadMarkerSource.OpenSprite

        width: pointId.markerView.spriteSize.width
        height: pointId.markerView.spriteSize.height
        anchors.centerIn: parent

        visible: !lead.completed || pointId.selected
        opacity: lead.completed ? Theme.completedLeadOpacity : 1.0

        QQ.TapHandler {
            gesturePolicy: QQ.TapHandler.ReleaseWithinBounds
            onTapped: (eventPoint) => {
                // Route through the manager so selecting this lead deselects any
                // other open lead; tapping the selected lead again closes it.
                // Ignore a tap that lands on a part of the marker hidden behind
                // geometry (the click must not pass through walls); closing is
                // always allowed. isOccluded tests the tapped pixel itself, so it
                // matches the silhouette the billboard is drawn with — map the tap
                // into the markerView's (viewport) coordinate space for the ray.
                // A view with no manager leaves its leads unselectable.
                if (pointId.selectionManager === null) {
                    return
                }
                if (pointId.selected) {
                    pointId.selectionManager.selectedItem = null
                } else {
                    let tap = hitTarget.mapToItem(pointId.markerView, eventPoint.position)
                    if (!pointId.markerView.isOccluded(pointId.position3D, tap)) {
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

            property bool editMode: false

            margin: Theme.sectionSpacing
            pointAtObject: hitTarget
            pointAtObjectPosition: Qt.point(Math.floor(hitTarget.width * .5),
                                            hitTarget.height)

            QQ.Item {
                id: popupContent

                // Optical pad between an icon and its text label; tightSpacing
                // alone reads too cramped at these small icon sizes.
                readonly property int iconLabelSpacing: Theme.tightSpacing + 2

                width: contentColumn.width
                height: contentColumn.height

                // Take the exclusive grab on any tap inside the popup so it can't
                // bubble up to the renderer's empty-space handler, which deselects
                // (and closes) the popup. A DragThreshold (passive) handler would
                // not stop propagation; WithinBounds grabs on press.
                QQ.TapHandler {
                    gesturePolicy: QQ.TapHandler.WithinBounds
                }

                ColumnLayout {
                    id: contentColumn
                    spacing: Theme.sectionSpacing

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: Theme.flowSpacing

                        QC.Label {
                            Layout.fillWidth: true
                            text: quoteBoxId.editMode ? "Edit Lead" : "Lead"
                            font.bold: true
                            font.pixelSize: Theme.fontSizeMedium
                        }

                        QQ.Rectangle {
                            objectName: "leadEditButton"
                            radius: Theme.floatingWidgetRadius
                            color: editHover.hovered ? Theme.hover : Theme.transparent
                            implicitWidth: editToggleRow.implicitWidth + Theme.flowSpacing * 2
                            implicitHeight: editToggleRow.implicitHeight + Theme.tightSpacing * 2

                            RowLayout {
                                id: editToggleRow
                                anchors.centerIn: parent
                                spacing: popupContent.iconLabelSpacing

                                Icon {
                                    sourceSize: Qt.size(13, 13)
                                    colorizationColor: Theme.textLink
                                    source: quoteBoxId.editMode
                                        ? "qrc:/twbs-icons/icons/check-lg.svg"
                                        : "qrc:/twbs-icons/icons/pencil.svg"
                                }

                                QC.Label {
                                    text: quoteBoxId.editMode ? "Done" : "Edit"
                                    color: Theme.textLink
                                    font.pixelSize: Theme.fontSizeSmall
                                }
                            }

                            QQ.HoverHandler {
                                id: editHover
                                cursorShape: Qt.PointingHandCursor
                            }

                            // ReleaseWithinBounds: button-style exclusive grab, so
                            // the tap toggles edit mode without bubbling up to the
                            // renderer's deselect handler.
                            QQ.TapHandler {
                                gesturePolicy: QQ.TapHandler.ReleaseWithinBounds
                                onTapped: quoteBoxId.editMode = !quoteBoxId.editMode
                            }
                        }
                    }

                    QQ.Rectangle {
                        Layout.fillWidth: true
                        implicitHeight: 1
                        color: Theme.divider
                    }

                    LeadInfoForm {
                        Layout.fillWidth: true
                        scrap: pointId.scrap
                        index: pointId.pointIndex
                        editable: quoteBoxId.editMode
                    }

                    QQ.Rectangle {
                        Layout.fillWidth: true
                        implicitHeight: 1
                        color: Theme.divider
                        visible: !quoteBoxId.editMode
                    }

                    RowLayout {
                        visible: !quoteBoxId.editMode
                        spacing: popupContent.iconLabelSpacing

                        Icon {
                            sourceSize: Qt.size(12, 12)
                            colorizationColor: Theme.textLink
                            source: "qrc:/twbs-icons/icons/arrow-up-right.svg"
                        }

                        LinkText {
                            objectName: "gotoNotes"
                            text: "Open in Notes"
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
    }

    QQ.Loader {
        sourceComponent: pointId.selected ? quoteBoxComponent : null
        asynchronous: true
    }
}

