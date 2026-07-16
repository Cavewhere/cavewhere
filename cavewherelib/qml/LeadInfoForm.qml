pragma ComponentBehavior: Bound

import QtQuick as QQ
import QtQuick.Layouts
import QtQuick.Controls as QC
import cavewherelib

// Shared lead field layout used by both the 2D notes editor (LeadInfoEditor)
// and the 3D view popup (LeadPoint). When editable is false the fields render
// as a clean read-only summary; when true they become inputs that commit edits
// back through the LeadListener.
GridLayout {
    id: editForm

    required property Scrap scrap
    property int index: -1
    property bool editable: false

    readonly property int valueWidth: 210
    readonly property int statusDotSize: 7

    columns: 2
    columnSpacing: Theme.columnGap
    rowSpacing: Theme.sectionSpacing

    component Caption: QC.Label {
        Layout.alignment: Qt.AlignLeft | Qt.AlignTop
        color: Theme.textSubtle
        font.pixelSize: Theme.fontSizeSmall
    }

    LeadListener {
        id: lead
        scrap: editForm.scrap
        index: editForm.index
    }

    Caption { text: "Status" }
    QQ.Loader {
        Layout.alignment: Qt.AlignLeft | Qt.AlignVCenter
        sourceComponent: editForm.editable ? completedEditComponent : completedViewComponent
    }

    Caption { text: "Size" }
    QQ.Loader {
        Layout.alignment: Qt.AlignLeft | Qt.AlignVCenter
        sourceComponent: editForm.editable ? sizeEditComponent : sizeViewComponent
    }

    Caption { text: "Description" }
    QQ.Loader {
        Layout.fillWidth: true
        Layout.alignment: Qt.AlignTop
        sourceComponent: editForm.editable ? descriptionEditComponent : descriptionViewComponent
    }

    QQ.Component {
        id: sizeViewComponent
        QC.Label {
            objectName: "sizeText"
            color: Theme.text
            font.pixelSize: Theme.fontSizeBody
            text: {
                if (lead.width === "?" && lead.height === "?") {
                    return "Not set"
                }
                let unitStr = (lead.unit >= 0 && lead.unit < lead.unitModel.length)
                    ? " " + lead.unitModel[lead.unit]
                    : ""
                return lead.width + " × " + lead.height + unitStr
            }
        }
    }

    QQ.Component {
        id: sizeEditComponent
        SizeEditor {
            readOnly: false
            unit: lead.unit
            unitModel: lead.unitModel
            widthText: lead.width
            heightText: lead.height
            onWidthFinishedEditting: (newText) => lead.width = (newText === "" ? "?" : newText)
            onHeightFinishedEditting: (newText) => lead.height = (newText === "" ? "?" : newText)
        }
    }

    QQ.Component {
        id: completedViewComponent
        QQ.Rectangle {
            id: statusPill
            objectName: "completedStatus"

            // An open (unexplored) lead is the actionable state, so it gets the
            // green highlight; a completed lead is "done", shown muted/grey.
            readonly property bool open: !lead.completed

            implicitWidth: statusRow.implicitWidth + Theme.flowSpacing * 2
            implicitHeight: statusRow.implicitHeight + Theme.tightSpacing * 2
            radius: height * 0.5
            color: statusPill.open ? Theme.success : Theme.surfaceMuted
            border.width: statusPill.open ? 0 : 1
            border.color: Theme.border

            RowLayout {
                id: statusRow
                anchors.centerIn: parent
                spacing: Theme.flowSpacing

                QQ.Rectangle {
                    Layout.alignment: Qt.AlignVCenter
                    implicitWidth: editForm.statusDotSize
                    implicitHeight: editForm.statusDotSize
                    radius: width * 0.5
                    color: statusPill.open ? Theme.textInverse : Theme.textSubtle
                }

                QC.Label {
                    text: statusPill.open ? "Open lead" : "Completed"
                    color: statusPill.open ? Theme.textInverse : Theme.textSubtle
                    font.pixelSize: Theme.fontSizeSmall
                }
            }
        }
    }

    QQ.Component {
        id: completedEditComponent
        QC.CheckBox {
            objectName: "completed"
            text: "Completed"

            // Two-way bound: the binding tracks the model, the handler writes
            // edits back. User clicks don't break the binding, so no extra
            // re-sync is needed.
            checked: lead.completed
            onCheckedChanged: lead.completed = checked
        }
    }

    QQ.Component {
        id: descriptionViewComponent
        QC.Label {
            objectName: "description"
            Layout.fillWidth: true
            Layout.preferredWidth: editForm.valueWidth
            wrapMode: QQ.Text.WordWrap
            font.pixelSize: Theme.fontSizeBody
            font.italic: lead.description.length === 0
            color: lead.description.length === 0 ? Theme.textSubtle : Theme.text
            text: lead.description.length === 0 ? "No description" : lead.description
        }
    }

    QQ.Component {
        id: descriptionEditComponent
        QC.TextArea {
            objectName: "description"
            Layout.fillWidth: true
            Layout.preferredWidth: editForm.valueWidth
            implicitHeight: 90
            placeholderText: "Lead's description"
            wrapMode: QQ.TextEdit.Wrap

            // Two-way bound: the binding tracks the model, the handler writes
            // edits back. Typing doesn't break the binding, so no extra re-sync
            // is needed.
            text: lead.description
            onTextChanged: lead.description = text
        }
    }
}
