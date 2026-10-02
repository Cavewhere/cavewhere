import QtQuick as QQ
import QtQuick.Templates as T
import QtQuick.Controls.impl
import cavewherelib

T.ComboBox {
    id: control

    implicitWidth: Math.max(implicitBackgroundWidth + leftInset + rightInset,
                            implicitContentWidth + leftPadding + rightPadding)
    implicitHeight: Math.max(implicitBackgroundHeight + topInset + bottomInset,
                             implicitContentHeight + topPadding + bottomPadding,
                             implicitIndicatorHeight + topPadding + bottomPadding)

    leftPadding: padding + (!control.mirrored || !indicator || !indicator.visible ? 0 : indicator.width + spacing)
    rightPadding: padding + (control.mirrored || !indicator || !indicator.visible ? 0 : indicator.width + spacing)
    opacity: control.enabled ? 1 : Theme.disabledOpacity

    delegate: ItemDelegate {
        required property var model
        required property int index

        width: QQ.ListView.view.width
        text: model[control.textRole]
        palette.text: control.palette.text
        palette.highlightedText: control.palette.highlightedText
        font.weight: control.currentIndex === index ? QQ.Font.DemiBold : QQ.Font.Normal
        highlighted: control.highlightedIndex === index
        hoverEnabled: control.hoverEnabled
    }

    // A square zone at the trailing edge holding the chevron.
    indicator: QQ.Item {
        x: control.mirrored ? 0 : control.width - width
        implicitWidth: Theme.controlHeight
        implicitHeight: Theme.controlHeight
        height: control.height

        QQ.Rectangle {
            anchors.fill: parent
            topLeftRadius: control.mirrored ? Theme.controlRadius : 0
            bottomLeftRadius: control.mirrored ? Theme.controlRadius : 0
            topRightRadius: control.mirrored ? 0 : Theme.controlRadius
            bottomRightRadius: control.mirrored ? 0 : Theme.controlRadius
            color: Theme.hoverOverlay
            visible: control.hovered
        }

        ColorImage {
            anchors.centerIn: parent
            width: Theme.chevronSize
            height: Theme.chevronSize
            sourceSize: Qt.size(width, height)
            source: "qrc:/twbs-icons/icons/chevron-down.svg"
            color: control.hovered ? control.palette.text : Theme.textSecondary
        }
    }

    contentItem: T.TextField {
        implicitHeight: contentHeight + topPadding + bottomPadding
        leftPadding: control.mirrored ? 0 : Theme.fieldHorizontalPadding
        rightPadding: control.mirrored ? Theme.fieldHorizontalPadding : 0
        topPadding: Theme.controlVerticalPadding - control.padding
        bottomPadding: Theme.controlVerticalPadding - control.padding

        text: control.editable ? control.editText : control.displayText

        enabled: control.editable
        autoScroll: control.editable
        readOnly: control.down
        inputMethodHints: control.inputMethodHints
        validator: control.validator
        selectByMouse: control.selectTextByMouse

        color: control.palette.text
        selectionColor: Theme.highlight
        selectedTextColor: control.palette.text
        verticalAlignment: QQ.Text.AlignVCenter
    }

    background: StyleFieldPanel {
        implicitWidth: Theme.compactFieldWidth
        implicitHeight: Theme.controlHeight
        hovered: control.hovered
        focused: control.editable ? control.activeFocus : control.visualFocus
        visible: !control.flat || control.down
    }

    popup: T.Popup {
        y: control.height + Theme.popupGap
        width: control.width
        height: Math.min(contentItem.implicitHeight + topPadding + bottomPadding,
                         control.QQ.Window.height - topMargin - bottomMargin)
        topMargin: Theme.popupShadowMargin
        bottomMargin: Theme.popupShadowMargin
        leftInset: -Theme.popupShadowMargin
        topInset: -Theme.popupShadowMargin
        rightInset: -Theme.popupShadowMargin
        bottomInset: -Theme.popupShadowMargin
        padding: Theme.popupPadding
        font: control.font

        contentItem: QQ.ListView {
            clip: true
            implicitHeight: contentHeight
            model: control.delegateModel
            currentIndex: control.highlightedIndex
            highlightMoveDuration: 0

            T.ScrollIndicator.vertical: ScrollIndicator { }
        }

        background: StylePopupPanel { }
    }
}
