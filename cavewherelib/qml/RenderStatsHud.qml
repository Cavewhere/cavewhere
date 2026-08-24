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

// Debug overlay listing render-resource bytes per category, driven by
// cwRenderingStatsModel. Shown only while the render-stats HUD setting is on.
QQ.Rectangle {
    id: hudRootId
    objectName: "renderStatsHud"

    readonly property real backgroundOpacity: 0.85
    readonly property int categoryColumnWidth: Math.round(140 * Theme.fontScale)
    readonly property int byteColumnWidth: Math.round(64 * Theme.fontScale)
    readonly property int separatorHeight: 1
    readonly property int bytesPerMegabyte: 1024 * 1024
    readonly property int gpuMemoryBudgetMb: RootData.settings.renderingSettings.gpuMemoryBudgetMb
    readonly property bool overBudget: statsModelId.totalGpuBytes > hudRootId.gpuMemoryBudgetMb * hudRootId.bytesPerMegabyte

    visible: RootData.settings.renderingSettings.showRenderStatsHud

    implicitWidth: layoutId.implicitWidth + Theme.statsPadding * 2
    implicitHeight: layoutId.implicitHeight + Theme.statsPadding * 2

    color: Qt.alpha(Theme.floatingWidgetColor, hudRootId.backgroundOpacity)
    radius: Theme.floatingWidgetRadius

    RenderingStatsModel {
        id: statsModelId
        running: hudRootId.visible
    }

    ColumnLayout {
        id: layoutId
        anchors.fill: parent
        anchors.margins: Theme.statsPadding
        spacing: Theme.tightSpacing

        QQ.Repeater {
            model: statsModelId

            delegate: RowLayout {
                id: rowId

                required property string name
                required property string gpuText
                required property string cpuText
                required property int cpuBytes

                spacing: Theme.flowSpacing

                QC.Label {
                    text: rowId.name
                    color: Theme.text
                    font.pixelSize: Theme.fontSizeCaption
                    Layout.preferredWidth: hudRootId.categoryColumnWidth
                }

                QC.Label {
                    text: rowId.gpuText
                    color: Theme.text
                    font.family: Theme.fontFamilyMono
                    font.pixelSize: Theme.fontSizeCaption
                    horizontalAlignment: QQ.Text.AlignRight
                    Layout.preferredWidth: hudRootId.byteColumnWidth
                }

                // CPU-resident bytes are the exception, so they only take space
                // in the row where a category actually holds some.
                QC.Label {
                    text: qsTr("(cpu %1)").arg(rowId.cpuText)
                    color: Theme.text
                    font.family: Theme.fontFamilyMono
                    font.pixelSize: Theme.fontSizeCaption
                    visible: rowId.cpuBytes > 0
                }
            }
        }

        QQ.Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: hudRootId.separatorHeight
            color: Theme.text
            opacity: hudRootId.backgroundOpacity
        }

        RowLayout {
            spacing: Theme.flowSpacing

            QC.Label {
                text: qsTr("GPU total / budget")
                color: Theme.text
                font.pixelSize: Theme.fontSizeCaption
                Layout.preferredWidth: hudRootId.categoryColumnWidth
            }

            // The budget is advisory for now: going over colors the total, and
            // nothing is evicted.
            QC.Label {
                objectName: "renderStatsHudTotal"
                text: statsModelId.totalGpuText
                color: hudRootId.overBudget ? Theme.warning : Theme.text
                font.family: Theme.fontFamilyMono
                font.pixelSize: Theme.fontSizeCaption
                horizontalAlignment: QQ.Text.AlignRight
                Layout.preferredWidth: hudRootId.byteColumnWidth
            }

            QC.Label {
                objectName: "renderStatsHudBudget"
                text: qsTr("/ %1 MB").arg(hudRootId.gpuMemoryBudgetMb)
                color: Theme.text
                font.family: Theme.fontFamilyMono
                font.pixelSize: Theme.fontSizeCaption
            }
        }

        // Frustum-culling counts for the last gathered frame, informational only.
        QC.Label {
            objectName: "renderStatsHudCulling"
            text: qsTr("Culled: %1/%2 objects · %3/%4 items")
                .arg(statsModelId.culledObjects)
                .arg(statsModelId.totalObjects)
                .arg(statsModelId.culledItems)
                .arg(statsModelId.totalItems)
            color: Theme.text
            font.pixelSize: Theme.fontSizeCaption
        }
    }
}
