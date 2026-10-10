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

    property bool popupOpen: false

    readonly property ColorBy colorBy: RootData.regionSceneManager.colorBy
    readonly property bool active: colorBy.source !== ColorBy.None
    readonly property bool depthActive: colorBy.source === ColorBy.Depth
    readonly property bool loopErrorActive: colorBy.source === ColorBy.LoopError
    readonly property bool showsBar: depthActive || loopErrorActive
    readonly property bool showsSwatches: colorBy.source === ColorBy.Keyword
    readonly property int stripCount: 48
    readonly property int barWidth: 14
    readonly property int barHeight: 200
    // Ticks span the bar and overhang its right edge by this much
    readonly property int tickOverhang: 4
    readonly property int tickThickness: 1
    readonly property int tickLabelGap: 6
    readonly property int tickMarkWidth: barWidth + tickOverhang
    readonly property int tickLabelX: tickMarkWidth + tickLabelGap
    // Sized from the end labels in the default label font, so the widest
    // depth or sigma label fits and the end labels center on the bar's end ticks.
    readonly property int labelHeight: Math.ceil(tickMetricsId.height)
    readonly property int labelWidth: Math.ceil(Math.max(tickMetricsId.advanceWidth(tickText(colorBy.rangeMin)),
                                                         tickMetricsId.advanceWidth(tickText(colorBy.rangeMax))))
    readonly property int endInset: Math.ceil(labelHeight / 2)
    // The pivot's label hides when its mark is closer than this to a rounded tick
    readonly property int minLabelSpacing: labelHeight
    readonly property int contentPadding: 6
    readonly property real backdropOpacity: 0.8
    readonly property int swatchSize: 12
    readonly property int swatchRowHeight: labelHeight
    readonly property int swatchSpacing: 2
    // Longer swatch lists scroll
    readonly property int maxVisibleSwatchRows: 12
    // Loop error's sigma scale has no pivot tick
    readonly property bool pivotInside: depthActive
                                        && colorBy.rangeMin < colorBy.pivot && colorBy.pivot < colorBy.rangeMax
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

    // Where a value sits along the bar, top (rangeMax) to bottom (rangeMin),
    // in legend coordinates (the bar starts endInset down)
    function tickY(value) {
        const span = colorBy.rangeMax - colorBy.rangeMin
        const fraction = span > 0 ? (colorBy.rangeMax - value) / span : 0.5
        return endInset + barHeight * fraction
    }

    // Rounded ticks always show their label; the pivot's label hides when it
    // would crowd a rounded tick's.
    function tickLabelVisible(value) {
        const ticks = colorBy.legendTicks
        let nearestDistance = Infinity
        for (let i = 0; i < ticks.length; ++i) {
            if (Math.abs(ticks[i] - value) < tickMatchTolerance) {
                return true
            }
            nearestDistance = Math.min(nearestDistance, Math.abs(tickY(ticks[i]) - tickY(value)))
        }
        return nearestDistance >= minLabelSpacing
    }

    // formatTick() is an invokable QML cannot track, so name the unit system
    // and source to re-evaluate when either changes.
    function tickText(value) {
        colorBy.unitSystem
        colorBy.source
        return colorBy.formatTick(value)
    }

    width: contentId.width
    height: contentId.height

    // Carries the default label font for tickMetricsId
    QC.Label {
        id: defaultFontLabelId

        visible: false
    }

    QQ.FontMetrics {
        id: tickMetricsId

        font: defaultFontLabelId.font
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
        // names the feature beside the pill that opens the controls.
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
                text: {
                    if (rootId.depthActive) {
                        return qsTr("Depth")
                    }
                    if (rootId.loopErrorActive) {
                        return qsTr("Loop error")
                    }
                    return rootId.colorBy.keywordKey
                }
                font.bold: true
            }

            QQ.Item {
                Layout.fillWidth: true
            }

            EditLinkButton {
                objectName: "colorByEditButton"

                active: rootId.popupOpen

                onClicked: rootId.clicked()
            }
        }

        // On state: the colorbar, with rounded depth or sigma ticks drawn across it and
        // labeled on its right.
        QQ.Item {
            id: legendId
            objectName: "colorByBar"

            visible: rootId.showsBar
            width: rootId.tickLabelX + rootId.labelWidth
            height: rootId.barHeight + 2 * rootId.endInset

            QQ.Column {
                y: rootId.endInset

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

            QQ.Repeater {
                objectName: "colorByTicks"

                model: rootId.tickValues

                QQ.Item {
                    id: tickId

                    required property real modelData

                    readonly property real markY: rootId.tickY(modelData)
                    readonly property bool labelVisible: rootId.tickLabelVisible(modelData)

                    width: legendId.width
                    height: legendId.height

                    // Clamped so the end ticks draw a full-thickness line on the bar's edges
                    QQ.Rectangle {
                        x: 0
                        y: Math.max(rootId.endInset,
                                    Math.min(rootId.endInset + rootId.barHeight - height, tickId.markY - height / 2))
                        width: rootId.tickMarkWidth
                        height: rootId.tickThickness
                        color: Theme.text
                    }

                    // Centered on its mark; the end inset keeps the end labels inside the legend
                    QC.Label {
                        visible: tickId.labelVisible
                        x: rootId.tickLabelX
                        y: tickId.markY - height / 2
                        text: rootId.tickText(tickId.modelData)
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
                            }
                        }
                    }
                }
            }
        }
    }
}
