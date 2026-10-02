import QtQuick as QQ
import QtQuick.Templates as T
import cavewherelib

T.GroupBox {
    id: control

    implicitWidth: Math.max(implicitBackgroundWidth + leftInset + rightInset,
                            implicitContentWidth + leftPadding + rightPadding,
                            implicitLabelWidth + leftPadding + rightPadding)
    implicitHeight: Math.max(implicitBackgroundHeight + topInset + bottomInset,
                             implicitContentHeight + topPadding + bottomPadding)

    spacing: Theme.groupBoxTitleSpacing
    horizontalPadding: Theme.groupBoxHorizontalPadding
    verticalPadding: Theme.containerPadding
    topPadding: verticalPadding + (implicitLabelWidth > 0 ? implicitLabelHeight + spacing : 0)

    label: T.Label {
        x: control.leftPadding
        width: control.availableWidth

        text: control.title
        font.bold: true
        color: control.palette.windowText
        elide: QQ.Text.ElideRight
        verticalAlignment: QQ.Text.AlignVCenter
    }

    background: QQ.Rectangle {
        color: Theme.surface
        border.width: 1
        border.color: Theme.border
        radius: Theme.panelRadius
    }

    // The title sits inside the card, so the style places every label at the
    // top padding, a label the app supplies in place of this one included.
    QQ.Binding {
        target: control.label
        property: "y"
        value: control.verticalPadding
    }
}
