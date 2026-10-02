/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

pragma ComponentBehavior: Bound

import QtQuick as QQ
import QtQuick.Controls as QC
import QtQuick.Layouts
import cavewherelib

// A survey node's warnings on its page, one line per warning (see
// NodeWarningModel for which ones). Tapping a line opens where the warning is
// fixed: a fix-station row (or the Fix Stations page with no row, for row -1),
// a trip's page, or this node's own source line, which lives on the hosting
// page and so is raised as sourceLineRequested().
//
// The host owns visibility and width: it shows the banner while count > 0 and
// gives it the width to wrap in. The banner only starts hidden, so it never
// fights a LayoutItemProxy hosting it.
QQ.Rectangle {
    id: bannerId
    objectName: "nodeWarningsBanner"

    property SurveyNode node: null

    readonly property int count: warningModelId.count

    signal sourceLineRequested()

    function openTarget(target: int, trip: Trip, fixStationRow: int) {
        switch (target) {
        case NodeWarningModel.FixStationRow: {
            // Only caves have a fix-station page so far; a page for a nested
            // node needs fixStationsLink to take a SurveyNode first.
            RootData.pageSelectionModel.currentPageAddress =
                    linkGeneratorId.fixStationsLink(bannerId.node as Cave)
            const page = RootData.pageSelectionModel.currentPage
            if (page !== null) {
                page.selectionProperties = { "currentFixRow": fixStationRow }
            }
            break
        }
        case NodeWarningModel.TripPage:
            RootData.pageSelectionModel.currentPageAddress = linkGeneratorId.tripLink(trip)
            break
        case NodeWarningModel.SourceLine:
            bannerId.sourceLineRequested()
            break
        }
    }

    color: Theme.errorBackground
    radius: Theme.bannerRadius
    implicitHeight: layoutId.implicitHeight + 2 * Theme.delegatePadding
    visible: false

    LinkGenerator {
        id: linkGeneratorId
        pageSelectionModel: RootData.pageSelectionModel
    }

    RowLayout {
        id: layoutId

        anchors.fill: parent
        anchors.margins: Theme.delegatePadding
        spacing: Theme.delegatePadding

        QQ.Image {
            Layout.alignment: Qt.AlignTop
            source: "qrc:icons/svg/stopSignError.svg"
            sourceSize: Qt.size(Theme.iconSizeButton, Theme.iconSizeButton)
        }

        ColumnLayout {
            Layout.fillWidth: true
            spacing: Theme.delegatePadding

            QQ.Repeater {
                model: NodeWarningModel {
                    id: warningModelId
                    node: bannerId.node
                }

                delegate: ColumnLayout {
                    id: entryId

                    required property int index
                    required property string message
                    required property string detail
                    required property int target
                    required property Trip trip
                    required property int fixStationRow

                    readonly property bool opensTarget: entryId.target !== NodeWarningModel.NoTarget

                    objectName: "nodeWarning." + entryId.index
                    Layout.fillWidth: true
                    spacing: 0

                    // Plain text: the messages quote user-typed station and
                    // file names, and markup would swallow a name like A<b>B.
                    QC.Label {
                        objectName: "nodeWarningMessage"
                        Layout.fillWidth: true
                        textFormat: QC.Label.PlainText
                        wrapMode: QC.Label.WordWrap
                        font.pixelSize: Theme.fontSizeBody
                        font.underline: entryId.opensTarget && entryHoverId.hovered
                        text: entryId.message
                    }

                    QC.Label {
                        objectName: "nodeWarningDetail"
                        Layout.fillWidth: true
                        visible: entryId.detail.length > 0
                        textFormat: QC.Label.PlainText
                        wrapMode: QC.Label.WordWrap
                        font.pixelSize: Theme.fontSizeSmall
                        text: entryId.detail
                    }

                    QQ.HoverHandler {
                        id: entryHoverId
                        enabled: entryId.opensTarget
                        cursorShape: Qt.PointingHandCursor
                    }

                    QQ.TapHandler {
                        enabled: entryId.opensTarget
                        onTapped: bannerId.openTarget(entryId.target, entryId.trip, entryId.fixStationRow)
                    }
                }
            }
        }
    }
}
