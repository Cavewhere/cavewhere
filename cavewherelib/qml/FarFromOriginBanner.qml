/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

import QtQuick as QQ
import QtQuick.Controls as QC
import QtQuick.Layouts
import cavewherelib

/**
  "No geographic coordinate system — the survey sits 3,041 km from the
  origin": the app-scope banner for a project with no frame whose solve
  lands far out, where the 3D view opens on empty space. One per window,
  registered into the overlay's banner strip.

  The condition is region.farFromOriginNotice; this only presents it. Show it
  frames the survey in the 3D view, and Fix Stations… opens the page where a
  system is chosen. ✕ hides the banner for this session until the condition
  clears, so a later relapse shows it again.
 */
QQ.Rectangle {
    id: root
    objectName: "farFromOriginBanner"

    readonly property FarFromOriginNotice notice: RootData.region.farFromOriginNotice

    // The overlay of whichever window this ended up in — where it registers.
    readonly property AppOverlay windowOverlay: root.WindowOverlay.overlay as AppOverlay

    property bool dismissed: false

    function distanceText(meters: real) : string {
        const unit = Units.lengthDisplayUnit(meters, ProjectUnits.unitSystem)
        const value = Units.convertLength(meters, Units.Meters, unit)
        return qsTr("%1 %2").arg(Number(value).toLocaleString(Qt.locale(), 'f', 0))
                            .arg(Units.lengthUnitName(unit))
    }

    function showSurvey() {
        RootData.pageSelectionModel.currentPageAddress = "View"
        const viewPage = RootData.pageView.currentPageItem as RenderingView
        viewPage?.turnTableInteraction.resetView()
    }

    implicitHeight: bannerRowId.implicitHeight + Theme.sectionSpacing * 2
    implicitWidth: bannerRowId.implicitWidth + Theme.sectionSpacing * 2
    visible: root.notice.shown && !root.dismissed
    color: Theme.warningSurface
    radius: Theme.floatingWidgetRadius
    border.width: Theme.bannerBorderWidth
    border.color: Theme.warningBorder

    onWindowOverlayChanged: root.windowOverlay?.addBanner(root)

    QQ.Connections {
        target: root.notice

        function onShownChanged() {
            if (!root.notice.shown) {
                root.dismissed = false
            }
        }
    }

    LinkGenerator {
        id: linkGeneratorId
        pageSelectionModel: RootData.pageSelectionModel
    }

    RowLayout {
        id: bannerRowId

        anchors.fill: parent
        anchors.margins: Theme.sectionSpacing
        spacing: Theme.flowSpacing

        QC.Label {
            objectName: "farFromOriginMessage"

            Layout.fillWidth: true
            wrapMode: QC.Label.WordWrap
            font.pixelSize: Theme.fontSizeBody
            text: qsTr("No geographic coordinate system — the survey sits %1 from the origin.")
                  .arg(root.distanceText(root.notice.distanceFromOrigin))
        }

        QC.Button {
            objectName: "farFromOriginShowButton"

            text: qsTr("Show it")
            onClicked: root.showSurvey()
        }

        QC.Button {
            objectName: "farFromOriginFixStationsButton"

            text: qsTr("Fix Stations…")
            onClicked: RootData.pageSelectionModel.currentPageAddress =
                       linkGeneratorId.fixStationsLink(RootData.region.rootNode)
        }

        QC.Button {
            objectName: "farFromOriginDismissButton"

            text: "✕"
            QC.ToolTip.text: qsTr("Hide this until the survey moves again")
            QC.ToolTip.delay: Theme.toolTipDelay
            QC.ToolTip.visible: hovered
            onClicked: root.dismissed = true
        }
    }
}
