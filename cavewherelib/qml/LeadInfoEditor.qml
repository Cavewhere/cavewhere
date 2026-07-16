import cavewherelib

FloatingGroupBox {
    id: groupBox
    objectName: "leadEditor"

    property ScrapLeadView leadView;

    title: "Lead Info"
    visible: leadView !== null && leadView.selectedItemIndex >= 0

    LeadInfoForm {
        editable: true
        scrap: groupBox.leadView !== null ? groupBox.leadView.scrap : null
        index: groupBox.leadView !== null ? groupBox.leadView.selectedItemIndex : -1
    }
}
