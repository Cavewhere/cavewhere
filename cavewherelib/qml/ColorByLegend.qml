/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

import QtQuick as QQ
import QtQuick.Controls as QC
import cavewherelib

QQ.Item {
    id: rootId
    objectName: "colorByLegend"

    readonly property ColorBy colorBy: RootData.regionSceneManager.colorBy
    readonly property bool active: colorBy.source !== ColorBy.None
    readonly property int stripCount: 48
    readonly property int barWidth: 14
    readonly property int barHeight: 160
    readonly property int labelWidth: 56
    readonly property int contentPadding: 6
    readonly property real backdropOpacity: 0.8
    readonly property bool pivotInside: colorBy.rangeMin < colorBy.pivot && colorBy.pivot < colorBy.rangeMax

    signal clicked()

    // The bar covers only [rangeMin, rangeMax] of the ramp, so a +100 / -500
    // cave shows a short warm tail above a long cool run, not a symmetric bar.
    function valueAtStrip(i) {
        return colorBy.rangeMax - (colorBy.rangeMax - colorBy.rangeMin) * (i + 0.5) / stripCount
    }

    // colorAt() and normalized() are invokables QML cannot track, so name the
    // properties they depend on to re-evaluate when any of them changes.
    function stripColor(i) {
        colorBy.rampId
        colorBy.pivot
        colorBy.source
        return colorBy.colorAt(colorBy.normalized(valueAtStrip(i)))
    }

    function metersText(value) {
        return qsTr("%1 m").arg(Math.round(value))
    }

    width: active ? legendId.width : chipId.width
    height: active ? legendId.height : chipId.height

    // A plain tap opens the controls; right-click too, for people who try it.
    QQ.TapHandler {
        acceptedButtons: Qt.LeftButton | Qt.RightButton
        onTapped: rootId.clicked()
    }

    QQ.Rectangle {
        anchors.fill: parent
        radius: Theme.floatingWidgetRadius
        color: Theme.surface
        opacity: rootId.backdropOpacity
    }

    // Off state: a compact chip that names the feature, so there is always
    // something to click when no legend is showing.
    QQ.Row {
        id: chipId
        objectName: "colorByChip"

        visible: !rootId.active
        padding: rootId.contentPadding
        spacing: rootId.contentPadding

        Icon {
            anchors.verticalCenter: parent.verticalCenter
            width: Theme.iconSizeButton
            height: Theme.iconSizeButton
            sourceSize: Qt.size(Theme.iconSizeButton, Theme.iconSizeButton)
            source: "qrc:/twbs-icons/icons/palette.svg"
        }

        QC.Label {
            anchors.verticalCenter: parent.verticalCenter
            text: qsTr("Color by: None")
            font.pixelSize: Theme.fontSizeSmall
        }
    }

    // On state: the colorbar with min / pivot / max labels.
    QQ.Row {
        id: legendId
        objectName: "colorByBar"

        visible: rootId.active
        padding: rootId.contentPadding
        spacing: Theme.linkBarButtonSpacing

        QQ.Column {
            QQ.Repeater {
                objectName: "colorByStrips"

                model: rootId.stripCount

                QQ.Rectangle {
                    required property int index

                    width: rootId.barWidth
                    height: rootId.barHeight / rootId.stripCount
                    color: rootId.stripColor(index)
                }
            }
        }

        QQ.Item {
            width: rootId.labelWidth
            height: rootId.barHeight

            QC.Label {
                anchors.top: parent.top
                text: rootId.metersText(rootId.colorBy.rangeMax)
                font.pixelSize: Theme.fontSizeCaption
            }

            QC.Label {
                visible: rootId.pivotInside
                y: parent.height * (rootId.colorBy.rangeMax - rootId.colorBy.pivot)
                   / (rootId.colorBy.rangeMax - rootId.colorBy.rangeMin) - height / 2
                text: rootId.metersText(rootId.colorBy.pivot)
                font.pixelSize: Theme.fontSizeCaption
            }

            QC.Label {
                anchors.bottom: parent.bottom
                text: rootId.metersText(rootId.colorBy.rangeMin)
                font.pixelSize: Theme.fontSizeCaption
            }
        }
    }
}
