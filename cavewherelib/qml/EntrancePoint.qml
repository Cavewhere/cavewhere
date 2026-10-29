pragma ComponentBehavior: Bound

import QtQuick as QQ
import QtQuick.Controls as QC
import QtQuick.Layouts
import cavewherelib

// The 2D stand-in for one entrance marker. The pin and its text draw as
// billboards of the entrance MarkerView's keyed layers; cwMarkerView reads this
// item's marker visibility for both and places the item at the entrance's
// solved position. Tapping the pin opens a popup with the cave icon and name,
// the entrance's label, its station, whether it is fixed, the cave's stats, and
// a link to the node that owns the station.
PositionItem {
    id: pointId
    objectName: "entrancePoint_" + stationName

    property SurveyNode node
    property SurveyNode caveNode
    property string stationName
    property string caveName
    property string label
    property bool fixed: false
    property bool selected: false //Set by selectionManager
    property string displayName
    required property SelectionManager selectionManager //Injected by cwMarkerView
    required property MarkerView markerView //Injected by cwMarkerView

    property alias marker: hitTarget

    // Raises the open popup above the other pins. The MarkerView's own z stays
    // as GLTerrainRenderer sets it, above the camera interaction.
    z: selected ? 1 : 0

    // Tap target sized to the pin and standing on the station, tip down, so the
    // hit area matches the drawn sprite.
    QQ.Item {
        id: hitTarget

        width: pointId.markerView.spriteSize.width
        height: pointId.markerView.spriteSize.height
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: parent.verticalCenter

        QQ.TapHandler {
            gesturePolicy: QQ.TapHandler.ReleaseWithinBounds
            onTapped: (eventPoint) => {
                // Tapping the selected pin closes its popup. Otherwise a tap
                // selects the pin only where it draws (isTappable), so a wall the
                // depth test hides it behind keeps the tap. A view with no
                // manager leaves its pins unselectable.
                if (pointId.selectionManager === null) {
                    return
                }
                if (pointId.selected) {
                    pointId.selectionManager.selectedItem = null
                } else {
                    let tap = hitTarget.mapToItem(pointId.markerView, eventPoint.position)
                    if (pointId.markerView.isTappable(pointId.position3D, tap)) {
                        pointId.selectionManager.selectedItem = pointId
                    }
                }
            }
        }
    }

    QQ.Component {
        id: quoteBoxComponent
        QuoteBox {
            objectName: "entranceQuoteBox"

            margin: Theme.sectionSpacing
            pointAtObject: hitTarget
            pointAtObjectPosition: Qt.point(Math.floor(hitTarget.width * 0.5),
                                            hitTarget.height)

            QQ.Item {
                width: contentColumn.width
                height: contentColumn.height

                // Grabs every tap inside the popup so it can't bubble up to the
                // renderer's empty-space handler, which closes the popup.
                QQ.TapHandler {
                    gesturePolicy: QQ.TapHandler.WithinBounds
                }

                ColumnLayout {
                    id: contentColumn
                    spacing: Theme.sectionSpacing

                    RowLayout {
                        spacing: Theme.entrancePopupIconSpacing

                        Icon {
                            objectName: "entranceIcon"
                            Layout.alignment: Qt.AlignVCenter
                            sourceSize: Qt.size(Theme.iconSizeButton, Theme.iconSizeButton)
                            colorizationColor: Theme.textSubtle
                            source: Theme.kindIconCave
                        }

                        QC.Label {
                            objectName: "entranceCaveName"
                            text: pointId.caveName
                            font.bold: true
                            font.pixelSize: Theme.fontSizeMedium
                        }
                    }

                    QC.Label {
                        objectName: "entranceLabel"
                        visible: pointId.label !== ""
                        text: pointId.label
                    }

                    QC.Label {
                        objectName: "entranceStation"
                        text: "Station: " + pointId.stationName
                    }

                    QC.Label {
                        objectName: "entranceFixed"
                        visible: pointId.fixed
                        text: "Fixed"
                    }

                    QQ.Rectangle {
                        Layout.fillWidth: true
                        implicitHeight: Theme.entrancePopupDividerThickness
                        color: Theme.divider
                    }

                    SelectableCaveStat {
                        objectName: "entranceLength"
                        label: "Length:"
                        unitValue: pointId.caveNode ? pointId.caveNode.length : null
                    }

                    SelectableCaveStat {
                        objectName: "entranceDepth"
                        label: "Depth:"
                        unitValue: pointId.caveNode ? pointId.caveNode.depth : null
                        depth: true
                    }

                    RowLayout {
                        spacing: Theme.entrancePopupIconSpacing

                        Icon {
                            sourceSize: Qt.size(Theme.entrancePopupIconSize, Theme.entrancePopupIconSize)
                            colorizationColor: Theme.textLink
                            source: "qrc:/twbs-icons/icons/arrow-up-right.svg"
                        }

                        LinkText {
                            objectName: "entranceNodeLink"
                            text: "Open " + (pointId.node ? pointId.node.name : "")
                            onClicked: RootData.pageSelectionModel.currentPageAddress = linkGenerator.nodeLink(pointId.node)

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
