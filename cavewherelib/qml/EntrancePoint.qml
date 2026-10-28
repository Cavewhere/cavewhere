pragma ComponentBehavior: Bound

import QtQuick as QQ
import QtQuick.Controls as QC
import QtQuick.Layouts
import cavewherelib

// The 2D stand-in for one entrance marker. The pin and its text draw as
// billboards of the EntranceView's keyed layers; cwEntranceView reads this
// item's marker visibility for both and places the item at the fix's solved
// position. Tapping the pin opens a popup with the fix's name, kind and its
// node's stats, and a link to the node's page.
PositionItem {
    id: pointId
    objectName: "entrancePoint_" + stationName

    property SurveyNode node
    property string stationName
    property string fixId
    property bool selected: false //Set by selectionManager
    property string displayName
    property int kind //A FixStation.Kind
    property string kindName
    required property SelectionManager selectionManager //Injected by cwEntranceView
    required property EntranceView entranceView //Injected by cwEntranceView

    property alias marker: hitTarget

    // Raises the open popup above the other pins. The EntranceView's own z stays
    // as GLTerrainRenderer sets it, above the camera interaction.
    z: selected ? 1 : 0

    // Tap target sized to the pin and standing on the station, tip down, so the
    // hit area matches the drawn sprite.
    QQ.Item {
        id: hitTarget

        width: pointId.entranceView.spriteSize.width
        height: pointId.entranceView.spriteSize.height
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: parent.verticalCenter

        QQ.TapHandler {
            gesturePolicy: QQ.TapHandler.ReleaseWithinBounds
            onTapped: (eventPoint) => {
                // Tapping the selected pin closes its popup. Otherwise a tap on
                // a pin pixel that cave geometry hides is ignored, so a click
                // can't reach through a wall.
                if (pointId.selected) {
                    pointId.selectionManager.selectedItem = null
                } else {
                    let tap = hitTarget.mapToItem(pointId.entranceView, eventPoint.position)
                    if (!pointId.entranceView.isOccluded(pointId.position3D, tap)) {
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

                    QC.Label {
                        objectName: "entranceName"
                        text: pointId.displayName
                        font.bold: true
                        font.pixelSize: Theme.fontSizeMedium
                    }

                    QC.Label {
                        objectName: "entranceKind"
                        text: pointId.kindName
                    }

                    QQ.Rectangle {
                        Layout.fillWidth: true
                        implicitHeight: Theme.entrancePopupDividerThickness
                        color: Theme.divider
                    }

                    SelectableCaveStat {
                        objectName: "entranceLength"
                        visible: pointId.kind === FixStation.Entrance
                        label: "Length:"
                        unitValue: pointId.node ? pointId.node.length : null
                    }

                    SelectableCaveStat {
                        objectName: "entranceDepth"
                        visible: pointId.kind === FixStation.Entrance
                        label: "Depth:"
                        unitValue: pointId.node ? pointId.node.depth : null
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
