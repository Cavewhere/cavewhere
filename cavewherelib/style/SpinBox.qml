import QtQuick as QQ
import QtQuick.Templates as T
import QtQuick.Controls.impl
import cavewherelib

T.SpinBox {
    id: control

    // The indicator column's width is part of the padding on its side.
    implicitWidth: Math.max(implicitBackgroundWidth + leftInset + rightInset,
                            contentItem.implicitWidth + leftPadding + rightPadding)
    implicitHeight: Math.max(implicitBackgroundHeight + topInset + bottomInset,
                             implicitContentHeight + topPadding + bottomPadding,
                             up.implicitIndicatorHeight + down.implicitIndicatorHeight)

    verticalPadding: Theme.controlVerticalPadding
    leftPadding: control.mirrored ? (up.indicator ? up.indicator.width : 0) : Theme.fieldHorizontalPadding
    rightPadding: control.mirrored ? Theme.fieldHorizontalPadding : (up.indicator ? up.indicator.width : 0)
    opacity: control.enabled ? 1 : Theme.disabledOpacity

    validator: QQ.IntValidator {
        locale: control.locale.name
        bottom: Math.min(control.from, control.to)
        top: Math.max(control.from, control.to)
    }

    contentItem: QQ.TextInput {
        z: 2
        text: control.displayText
        clip: width < implicitWidth

        font: control.font
        color: control.palette.text
        selectionColor: Theme.highlight
        selectedTextColor: control.palette.text
        horizontalAlignment: Qt.AlignLeft
        verticalAlignment: Qt.AlignVCenter

        readOnly: !control.editable
        validator: control.validator
        inputMethodHints: control.inputMethodHints

        T.ContextMenu.menu: StyleTextEditingMenu {
            editor: control.contentItem
        }
    }

    // Each zone sits one border width inside the field's outline. A zone is
    // disabled at its end of the range; enabled here is the zone's own.
    up.indicator: QQ.Rectangle {
        x: control.mirrored ? 1 : control.width - width - 1
        y: 1
        implicitWidth: Theme.spinIndicatorWidth
        implicitHeight: Theme.controlHeight / 2
        height: control.height / 2 - 1
        topLeftRadius: control.mirrored ? Theme.controlRadius - 1 : 0
        topRightRadius: control.mirrored ? 0 : Theme.controlRadius - 1
        color: control.up.pressed ? Theme.popupSelected
             : control.up.hovered ? Theme.hoverOverlay : "transparent"
        opacity: control.enabled && !enabled ? Theme.disabledOpacity : 1

        ColorImage {
            anchors.centerIn: parent
            width: Theme.spinChevronSize
            height: Theme.spinChevronSize
            sourceSize: Qt.size(width, height)
            source: "qrc:/twbs-icons/icons/chevron-up.svg"
            color: control.up.hovered ? control.palette.text : Theme.textSecondary
        }
    }

    down.indicator: QQ.Rectangle {
        x: control.mirrored ? 1 : control.width - width - 1
        y: control.height / 2
        implicitWidth: Theme.spinIndicatorWidth
        implicitHeight: Theme.controlHeight / 2
        height: control.height / 2 - 1
        bottomLeftRadius: control.mirrored ? Theme.controlRadius - 1 : 0
        bottomRightRadius: control.mirrored ? 0 : Theme.controlRadius - 1
        color: control.down.pressed ? Theme.popupSelected
             : control.down.hovered ? Theme.hoverOverlay : "transparent"
        opacity: control.enabled && !enabled ? Theme.disabledOpacity : 1

        ColorImage {
            anchors.centerIn: parent
            width: Theme.spinChevronSize
            height: Theme.spinChevronSize
            sourceSize: Qt.size(width, height)
            source: "qrc:/twbs-icons/icons/chevron-down.svg"
            color: control.down.hovered ? control.palette.text : Theme.textSecondary
        }
    }

    background: StyleFieldPanel {
        implicitWidth: Theme.compactFieldWidth
        implicitHeight: Theme.controlHeight
        hovered: control.hovered
        focused: control.editable ? control.activeFocus : control.visualFocus
    }
}
