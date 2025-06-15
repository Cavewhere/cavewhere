import QtQuick as QQ
import QtQuick.Layouts
import QtQuick.Controls as QC
import cavewherelib

FloatingGroupBox {
    id: groupBox
    objectName: "leadEditor"

    property ScrapLeadView leadView;

    title: "Lead Info"
    visible: leadView !== null && leadView.selectedItemIndex >= 0

    ColumnLayout {

        LeadListener {
            id: lead
            scrap: groupBox.leadView !== null ? groupBox.leadView.scrap : null
            index: groupBox.leadView !== null ? groupBox.leadView.selectedItemIndex : -1
        }

        SizeEditor {
            id: sizeEditor
            onWidthFinishedEditting: (newText) => lead.width = lead.toLeadDim(newText)
            onHeightFinishedEditting: (newText) => lead.height = lead.toLeadDim(newText)
            unit: lead.unit
            unitModel: lead.unitModel
            widthText: lead.width
            heightText: lead.height
            nextTab: leadDescriptionArea
        }

        QC.TextArea {
            id: leadDescriptionArea
            objectName: "description"
            QQ.KeyNavigation.backtab: sizeEditor.heightTextObject
            implicitHeight: 100
            implicitWidth: 250
            onTextChanged: lead.description = text
            text: lead.description
            placeholderText: "Lead's description"

            QQ.Connections {
                target: lead
                function onDescriptionChanged() {
                    leadDescriptionArea.text = lead.description
                }
            }
        }

        // LeadQuickComments {
        //     Layout.fillWidth: true
        //     category: "Passage Type"
        //     keywords: ["Aven", "Borehole", "Caynon", "Dig", "Dome", "Phreatic", "Pit", "Sump",]
        //     textArea: leadDescriptionArea
        // }

        // LeadQuickComments {
        //     category: "Passage Fill"
        //     Layout.fillWidth: true
        //     keywords: ["Breakdown", "Cobbles", "Muddy", "Sandy", "Water"]
        //     textArea: leadDescriptionArea
        // }e

        // LeadQuickComments {
        //     category: "Passage Traversal"
        //     Layout.fillWidth: true
        //     keywords: ["Crawl", "Climb", "Low", "High", "Swim", "Tight", "Traverse", "Walking"]
        //     textArea: leadDescriptionArea
        // }

        // LeadQuickComments {
        //     Layout.fillWidth: true
        //     category: "Stream Type"
        //     keywords: ["Large", "Stream", "Small", "Trickle"]
        //     textArea: leadDescriptionArea
        // }

        // LeadQuickComments {
        //     Layout.fillWidth: true
        //     category: "Air"
        //     keywords: ["Strong", "Air", "Weak"]
        //     textArea: leadDescriptionArea
        // }

        // LeadQuickComments {
        //     Layout.fillWidth: true
        //     category: "Gear Needed"
        //     keywords: ["Bolts", "Cable Ladder", "Handline", "Hammer", "Rope", "Dig Equipment"]
        //     textArea: leadDescriptionArea
        // }
    }
}

