import QtQuick 2.0 as QQ
import QtQuick.Layouts 1.1
import QtQuick.Controls
import cavewherelib

FloatingGroupBox {
    id: groupBox

    property ScrapLeadView leadView;

    title: "Lead Info"
    visible: leadView !== null && leadView.selectedItemIndex >= 0

    ColumnLayout {

        LeadListener {
            id: lead
            scrap: leadView !== null ? leadView.scrap : null
            index: leadView !== null ? leadView.selectedItemIndex : -1
        }

        SizeEditor {
            id: sizeEditor
            onWidthFinishedEditting: lead.width = lead.toLeadDim(newText)
            onHeightFinishedEditting: lead.height = lead.toLeadDim(newText)
            unit: lead.unit
            unitModel: lead.unitModel
            widthText: lead.width
            heightText: lead.height
            nextTab: leadDescriptionArea
        }

        TextArea {
            id: leadDescriptionArea
            QQ.KeyNavigation.backtab: sizeEditor.heightTextObject
            implicitHeight: 100
            onTextChanged: lead.description = text
            text: lead.description

            QQ.Connections {
                target: lead
                onDescriptionChanged: {
                    leadDescriptionArea.text = lead.description
                }
            }
        }

        LeadQuickComments {
            Layout.fillWidth: true
            category: "Passage Type"
            keywords: ["Aven", "Borehole", "Caynon", "Dig", "Dome", "Phreatic", "Pit", "Sump",]
            textArea: leadDescriptionArea
        }

        LeadQuickComments {
            category: "Passage Fill"
            Layout.fillWidth: true
            keywords: ["Breakdown", "Cobbles", "Muddy", "Sandy", "Water"]
            textArea: leadDescriptionArea
        }

        LeadQuickComments {
            category: "Passage Traversal"
            Layout.fillWidth: true
            keywords: ["Crawl", "Climb", "Low", "High", "Swim", "Tight", "Traverse", "Walking"]
            textArea: leadDescriptionArea
        }

        LeadQuickComments {
            Layout.fillWidth: true
            category: "Stream Type"
            keywords: ["Large", "Stream", "Small", "Trickle"]
            textArea: leadDescriptionArea
        }

        LeadQuickComments {
            Layout.fillWidth: true
            category: "Air"
            keywords: ["Strong", "Air", "Weak"]
            textArea: leadDescriptionArea
        }

        LeadQuickComments {
            Layout.fillWidth: true
            category: "Gear Needed"
            keywords: ["Bolts", "Cable Ladder", "Handline", "Hammer", "Rope", "Dig Equipment"]
            textArea: leadDescriptionArea
        }
    }
}

