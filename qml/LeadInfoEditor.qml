import QtQuick 2.0
import QtQuick.Layouts 1.1
import QtQuick.Controls 1.2
import Cavewhere 1.0

FloatingGroupBox {
    id: groupBox

    property ScrapLeadView leadView;

    title: qsTr("Lead Info")
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
            KeyNavigation.backtab: sizeEditor.heightTextObject
            implicitHeight: 100
            onTextChanged: lead.description = text
            text: lead.description

            Connections {
                target: lead
                onDescriptionChanged: {
                    leadDescriptionArea.text = lead.description
                }
            }
        }

        LeadQuickComments {
            Layout.fillWidth: true
            category: qsTr("Passage Type")
            keywords: ["Aven", "Borehole", "Caynon", "Dig", "Dome", "Phreatic", "Pit", "Sump",]
            textArea: leadDescriptionArea
        }

        LeadQuickComments {
            category: qsTr("Passage Fill")
            Layout.fillWidth: true
            keywords: ["Breakdown", "Cobbles", "Muddy", "Sandy", "Water"]
            textArea: leadDescriptionArea
        }

        LeadQuickComments {
            category: qsTr("Passage Traversal")
            Layout.fillWidth: true
            keywords: ["Crawl", "Climb", "Low", "High", "Swim", "Tight", "Traverse", "Walking"]
            textArea: leadDescriptionArea
        }

        LeadQuickComments {
            Layout.fillWidth: true
            category: qsTr("Stream Type")
            keywords: ["Large", "Stream", "Small", "Trickle"]
            textArea: leadDescriptionArea
        }

        LeadQuickComments {
            Layout.fillWidth: true
            category: qsTr("Air")
            keywords: ["Strong", "Air", "Weak"]
            textArea: leadDescriptionArea
        }

        LeadQuickComments {
            Layout.fillWidth: true
            category: qsTr("Gear Needed")
            keywords: ["Bolts", "Cable Ladder", "Handline", "Hammer", "Rope", "Dig Equipment"]
            textArea: leadDescriptionArea
        }
    }
}

