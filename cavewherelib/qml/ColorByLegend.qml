/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

import QtQuick as QQ
import QtQuick.Layouts
import QtQuick.Controls as QC
import cavewherelib

QQ.Item {
    id: rootId
    objectName: "colorByLegend"

    readonly property ColorBy colorBy: RootData.regionSceneManager.colorBy
    readonly property bool active: colorBy.source !== ColorBy.None
    readonly property bool showsBar: colorBy.source === ColorBy.Depth
    readonly property bool showsSwatches: colorBy.source === ColorBy.Keyword
    readonly property int stripCount: 48
    readonly property int barWidth: 14
    readonly property int barHeight: 200
    readonly property int tickLength: 4
    readonly property int tickThickness: 1
    readonly property int tickLabelGap: 2
    readonly property int labelWidth: 56
    readonly property int contentPadding: 6
    readonly property real backdropOpacity: 0.8
    readonly property int swatchSize: 12
    readonly property int swatchRowHeight: 16
    readonly property int swatchSpacing: 2
    // Longer swatch lists scroll
    readonly property int maxVisibleSwatchRows: 12
    readonly property bool pivotInside: colorBy.rangeMin < colorBy.pivot && colorBy.pivot < colorBy.rangeMax
    // Ticks come back from feet as meters, so match the pivot within a millimeter
    readonly property real tickMatchTolerance: 0.001
    // The rounded ticks, plus the pivot as its own labeled tick when it falls
    // between them.
    readonly property list<real> tickValues: {
        let values = []
        let pivotIsTick = false
        for (let i = 0; i < colorBy.legendTicks.length; ++i) {
            const value = colorBy.legendTicks[i]
            values.push(value)
            if (Math.abs(value - colorBy.pivot) < tickMatchTolerance) {
                pivotIsTick = true
            }
        }
        if (pivotInside && !pivotIsTick) {
            values.push(colorBy.pivot)
        }
        return values
    }

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

    // Where a value sits along the bar, top (rangeMax) to bottom (rangeMin)
    function tickY(value) {
        const span = colorBy.rangeMax - colorBy.rangeMin
        if (span <= 0) {
            return barHeight / 2
        }
        return barHeight * (colorBy.rangeMax - value) / span
    }

    // formatDepth() is an invokable QML cannot track, so name the unit system
    // to re-evaluate when it changes.
    function tickText(value) {
        colorBy.unitSystem
        return colorBy.formatDepth(value)
    }

    width: contentId.width
    height: contentId.height

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

    QQ.Column {
        id: contentId

        readonly property real bodyWidth: rootId.showsBar ? legendId.width
                                          : (rootId.showsSwatches ? swatchLegendId.width : 0)

        padding: rootId.contentPadding
        spacing: rootId.contentPadding

        // The title line names the source on the left and carries the Edit
        // pill on the right. Off, it is the whole widget: a compact chip that
        // names the feature, so there is always something to click.
        RowLayout {
            id: titleLineId
            objectName: "colorByTitleLine"

            width: Math.max(implicitWidth, contentId.bodyWidth)
            spacing: rootId.contentPadding

            QQ.Row {
                id: chipId
                objectName: "colorByChip"

                visible: !rootId.active
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

            QC.Label {
                objectName: "colorByTitle"

                visible: rootId.active
                text: rootId.showsBar ? qsTr("Depth") : rootId.colorBy.keywordKey
                font.pixelSize: Theme.fontSizeSmall
                font.bold: true
            }

            QQ.Item {
                Layout.fillWidth: true
            }

            EditLinkButton {
                objectName: "colorByEditButton"

                onClicked: rootId.clicked()
            }
        }

        // On state: the colorbar with rounded depth ticks.
        QQ.Row {
            id: legendId
            objectName: "colorByBar"

            visible: rootId.showsBar
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

                QQ.Repeater {
                    objectName: "colorByTicks"

                    model: rootId.tickValues

                    QQ.Item {
                        id: tickId

                        required property real modelData

                        readonly property real markY: rootId.tickY(modelData)

                        width: parent.width
                        height: rootId.barHeight

                        QQ.Rectangle {
                            x: 0
                            y: tickId.markY - height / 2
                            width: rootId.tickLength
                            height: rootId.tickThickness
                            color: Theme.text
                        }

                        // Centered on its mark, clamped so the end labels stay inside the bar
                        QC.Label {
                            x: rootId.tickLength + rootId.tickLabelGap
                            y: Math.max(0, Math.min(tickId.height - height, tickId.markY - height / 2))
                            text: rootId.tickText(tickId.modelData)
                            font.pixelSize: Theme.fontSizeCaption
                        }
                    }
                }
            }
        }

        // Keyword state: one swatch per value in slot order.
        QQ.Column {
            id: swatchLegendId
            objectName: "colorBySwatchLegend"

            visible: rootId.showsSwatches
            spacing: rootId.contentPadding

            QC.ScrollView {
                readonly property int rowCount: rootId.colorBy.legendValues.length

                width: contentWidth + effectiveScrollBarWidth
                height: Math.min(rowCount, rootId.maxVisibleSwatchRows)
                        * (rootId.swatchRowHeight + rootId.swatchSpacing)

                QQ.Column {
                    spacing: rootId.swatchSpacing

                    QQ.Repeater {
                        objectName: "colorBySwatches"

                        model: rootId.colorBy.legendValues

                        QQ.Row {
                            id: swatchRowId

                            required property string modelData
                            required property int index

                            height: rootId.swatchRowHeight
                            spacing: rootId.contentPadding

                            QQ.Rectangle {
                                anchors.verticalCenter: parent.verticalCenter
                                width: rootId.swatchSize
                                height: rootId.swatchSize
                                color: rootId.colorBy.slotColor(rootId.colorBy.legendSlot(swatchRowId.index))
                            }

                            QC.Label {
                                anchors.verticalCenter: parent.verticalCenter
                                text: swatchRowId.modelData
                                font.pixelSize: Theme.fontSizeCaption
                            }
                        }
                    }
                }
            }
        }
    }
}
