/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

pragma ComponentBehavior: Bound

import QtQuick as QQ
import QtQuick.Controls as QC
import QtQuick.Layouts
import cavewherelib

// Every control in every state, on one page. The before-and-after reference for
// the CaveWhere style, reached from Debug > Style Gallery.
StandardPage {
    id: galleryPage
    objectName: "styleGalleryPage"

    readonly property int cardWidth: 380
    readonly property int textAreaHeight: 90
    readonly property int scrollDemoHeight: 120
    readonly property int scrollDemoContentHeight: 400
    readonly property int splitDemoHeight: 80
    readonly property int smallBusySize: 16
    readonly property int quoteTipHeight: 30
    readonly property int quoteTipInset: 30
    readonly property int legacyGroupContentHeight: 40
    readonly property int contextButtonSize: 20
    readonly property int verticalTabsWidth: 160

    // The cards are drawn by hand so the gallery's own frame stays put while
    // GroupBox and Frame are restyled.
    component GalleryCard: QQ.Rectangle {
        id: cardId

        property string title
        default property alias content: cardContentId.data

        width: galleryPage.cardWidth
        implicitHeight: cardLayoutId.implicitHeight + 2 * Theme.pageMargin
        color: Theme.surface
        border.width: 1
        border.color: Theme.border
        radius: Theme.floatingWidgetRadius

        ColumnLayout {
            id: cardLayoutId
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.margins: Theme.pageMargin
            spacing: Theme.sectionSpacing

            QC.Label {
                text: cardId.title
                font.bold: true
                font.pixelSize: Theme.fontSizeSmall
            }

            ColumnLayout {
                id: cardContentId
                Layout.fillWidth: true
                spacing: Theme.sectionSpacing
            }
        }
    }

    function openMenu() {
        galleryMenuId.popup(menuButtonId, 0, menuButtonId.height)
    }

    function openDialog() {
        removeDialogId.open()
    }

    function closePopups() {
        galleryMenuId.close()
        removeDialogId.close()
    }

    function fontEntryIndex(family: string) : int {
        const entries = RootData.settings.fontSettings.fontEntries
        for (let i = 0; i < entries.length; ++i) {
            if (entries[i].family === family) {
                return i
            }
        }
        return 0
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Theme.pageMargin
        spacing: Theme.sectionSpacing

        // Header strip: scheme, enabled state, and font family for the cards
        RowLayout {
            objectName: "styleGalleryHeader"
            Layout.fillWidth: true
            spacing: Theme.columnGap

            QC.Label {
                text: "Style Gallery"
                font.bold: true
                font.pixelSize: Theme.fontSizeTitle
            }

            QC.ButtonGroup { id: schemeGroupId }

            QC.RadioButton {
                objectName: "gallerySystemRadioButton"
                text: "System"
                checked: RootData.settings.appearanceSettings.colorScheme === AppearanceSettings.System
                QC.ButtonGroup.group: schemeGroupId
                onClicked: RootData.settings.appearanceSettings.colorScheme = AppearanceSettings.System
            }

            QC.RadioButton {
                objectName: "galleryLightRadioButton"
                text: "Light"
                checked: RootData.settings.appearanceSettings.colorScheme === AppearanceSettings.Light
                QC.ButtonGroup.group: schemeGroupId
                onClicked: RootData.settings.appearanceSettings.colorScheme = AppearanceSettings.Light
            }

            QC.RadioButton {
                objectName: "galleryDarkRadioButton"
                text: "Dark"
                checked: RootData.settings.appearanceSettings.colorScheme === AppearanceSettings.Dark
                QC.ButtonGroup.group: schemeGroupId
                onClicked: RootData.settings.appearanceSettings.colorScheme = AppearanceSettings.Dark
            }

            QC.Switch {
                id: enabledSwitchId
                objectName: "galleryEnabledSwitch"
                text: "Enabled"
                checked: true
            }

            QC.ComboBox {
                objectName: "galleryFontComboBox"
                model: RootData.settings.fontSettings.fontEntries
                textRole: "label"
                currentIndex: galleryPage.fontEntryIndex(RootData.settings.fontSettings.fontFamily)
                onActivated: (index) => {
                    RootData.settings.fontSettings.fontFamily = RootData.settings.fontSettings.fontEntries[index].family
                }
            }

            QQ.Item { Layout.fillWidth: true }
        }

        QC.ScrollView {
            id: galleryScrollViewId
            objectName: "styleGalleryScrollView"
            Layout.fillWidth: true
            Layout.fillHeight: true
            contentWidth: availableWidth
            contentHeight: cardFlowId.implicitHeight

            QQ.Flow {
                id: cardFlowId
                objectName: "styleGalleryFlow"
                width: galleryScrollViewId.availableWidth
                spacing: Theme.columnGap
                enabled: enabledSwitchId.checked

                GalleryCard {
                    objectName: "cardButtons"
                    title: "Buttons"

                    QQ.Flow {
                        Layout.fillWidth: true
                        spacing: Theme.flowSpacing

                        QC.Button { text: "Export" }
                        QC.Button { text: "Save"; highlighted: true }
                        QC.Button { text: "Draw Up"; icon.source: "qrc:/twbs-icons/icons/plus.svg" }
                        QC.Button { text: "Flat"; flat: true }
                    }

                    QQ.Flow {
                        Layout.fillWidth: true
                        spacing: Theme.flowSpacing

                        QC.Button { text: "Neutral" }
                        QC.Button { text: "Down"; down: true }
                        QC.Button { text: "Checked"; checkable: true; checked: true }
                        QC.Button { text: "Disabled"; enabled: false }
                        QC.Button { text: "Delete"; palette.buttonText: Theme.errorText }
                    }

                    QQ.Flow {
                        Layout.fillWidth: true
                        spacing: Theme.flowSpacing

                        QC.ToolButton { icon.source: "qrc:/twbs-icons/icons/pencil.svg" }
                        QC.ToolButton {
                            icon.source: "qrc:/twbs-icons/icons/eraser.svg"
                            checkable: true
                            checked: true
                        }
                        QC.ToolButton { text: "Tool" }
                        QC.ToolButton {
                            icon.source: "qrc:/twbs-icons/icons/trash.svg"
                            enabled: false
                        }
                        QC.RoundButton { icon.source: "qrc:/twbs-icons/icons/plus.svg" }
                        RoundButton { icon.source: "qrc:/twbs-icons/icons/search.svg" }
                        RoundButton {
                            icon.source: "qrc:/twbs-icons/icons/caret-down-fill.svg"
                            implicitWidth: galleryPage.contextButtonSize
                            implicitHeight: galleryPage.contextButtonSize
                            radius: 0
                        }
                    }

                    QC.DialogButtonBox {
                        Layout.fillWidth: true
                        standardButtons: QC.DialogButtonBox.Ok | QC.DialogButtonBox.Cancel
                    }
                }

                GalleryCard {
                    objectName: "cardToggles"
                    title: "Toggles"

                    QC.ButtonGroup { id: viewGroupId }

                    RowLayout {
                        spacing: Theme.flowSpacing

                        QC.Button {
                            text: "Plan"
                            checkable: true
                            checked: true
                            QC.ButtonGroup.group: viewGroupId
                        }

                        QC.Button {
                            text: "Profile"
                            checkable: true
                            QC.ButtonGroup.group: viewGroupId
                        }
                    }

                    QC.TabBar {
                        Layout.fillWidth: true

                        QC.TabButton { text: "Trips" }
                        QC.TabButton { text: "Notes" }
                        QC.TabButton { text: "Leads" }
                    }

                    // Tab buttons outside a tab bar, as on the Settings page.
                    TabViewVertical {
                        Layout.preferredWidth: galleryPage.verticalTabsWidth
                        Layout.preferredHeight: contentHeight
                        model: ["General", "Appearance", "Units"]
                    }
                }

                GalleryCard {
                    objectName: "cardIndicators"
                    title: "Indicators"

                    QQ.Flow {
                        Layout.fillWidth: true
                        spacing: Theme.flowSpacing

                        QC.CheckBox { text: "Checked"; checked: true }
                        QC.CheckBox { text: "Unchecked" }
                        QC.CheckBox {
                            text: "Partial"
                            tristate: true
                            checkState: Qt.PartiallyChecked
                        }
                        QC.CheckBox { text: "Disabled"; checked: true; enabled: false }
                    }

                    QQ.Flow {
                        Layout.fillWidth: true
                        spacing: Theme.flowSpacing

                        QC.RadioButton { text: "Checked"; checked: true }
                        QC.RadioButton { text: "Unchecked" }
                        QC.RadioButton { text: "Disabled"; enabled: false }
                    }

                    QQ.Flow {
                        Layout.fillWidth: true
                        spacing: Theme.flowSpacing

                        QC.Switch { text: "On"; checked: true }
                        QC.Switch { text: "Off" }
                        QC.Switch { text: "Disabled"; enabled: false }
                    }
                }

                GalleryCard {
                    objectName: "cardFields"
                    title: "Fields"

                    QQ.Flow {
                        Layout.fillWidth: true
                        spacing: Theme.flowSpacing

                        QC.ComboBox { model: ["Meters", "Feet", "Yards", "Fathoms", "Chains"] }
                        QC.ComboBox {
                            editable: true
                            model: ["Compass", "Survex", "Walls", "Toporobot", "Therion"]
                        }
                        QC.ComboBox { enabled: false; model: ["Disabled"] }
                    }

                    QQ.Flow {
                        Layout.fillWidth: true
                        spacing: Theme.flowSpacing

                        QC.SpinBox { value: 12 }
                        DoubleSpinBox { realValue: 3.25 }
                    }

                    QQ.Flow {
                        Layout.fillWidth: true
                        spacing: Theme.flowSpacing

                        QC.TextField { placeholderText: "Station name" }
                        QC.TextField {
                            objectName: "galleryTextField"
                            text: "A42"
                        }
                        QC.TextField { text: "Read only"; readOnly: true }
                        QC.TextField { text: "Disabled"; enabled: false }
                    }

                    QC.ScrollView {
                        Layout.fillWidth: true
                        Layout.preferredHeight: galleryPage.textAreaHeight

                        QC.TextArea {
                            text: "Entrance in a sink at the edge of the woods.\n"
                                  + "Crawl for twenty meters to a junction.\n"
                                  + "Left lead ends in breakdown.\n"
                                  + "Right lead continues as a canyon.\n"
                                  + "Survey stopped at station A42."
                        }
                    }
                }

                GalleryCard {
                    objectName: "cardProgress"
                    title: "Progress"

                    QC.Slider {
                        Layout.fillWidth: true
                        value: 0.4
                    }

                    QC.ProgressBar {
                        Layout.fillWidth: true
                        value: 0.7
                    }

                    QC.ProgressBar {
                        Layout.fillWidth: true
                        indeterminate: true
                    }

                    RowLayout {
                        spacing: Theme.columnGap

                        QC.BusyIndicator { running: busyRunningSwitch.checked }

                        QC.BusyIndicator {
                            Layout.preferredWidth: galleryPage.smallBusySize
                            Layout.preferredHeight: galleryPage.smallBusySize
                            running: busyRunningSwitch.checked
                        }

                        TaskProgressRing { progress: 0.4 }

                        // Turning it back on plays the droplet opening on the larger ring.
                        QC.Switch {
                            id: busyRunningSwitch
                            text: "Running"
                            checked: true
                        }
                    }
                }

                GalleryCard {
                    objectName: "cardMenus"
                    title: "Menus"

                    // The tip sits beside the button so it pops up clear of it.
                    RowLayout {
                        spacing: Theme.columnGap

                        QC.Button {
                            id: menuButtonId
                            objectName: "galleryMenuButton"
                            text: "Open menu"
                            onClicked: galleryPage.openMenu()

                            QC.Menu {
                                id: galleryMenuId
                                objectName: "galleryMenu"
                                popupType: QC.Popup.Item

                                QC.MenuItem { text: "Copy" }
                                QC.MenuItem { text: "Show grid"; checkable: true; checked: true }
                                QC.MenuItem { text: "Show labels"; checkable: true }
                                QC.MenuSeparator { }
                                QC.MenuItem { text: "Disabled"; enabled: false }

                                QC.Menu {
                                    title: "Export"
                                    popupType: QC.Popup.Item

                                    QC.MenuItem { text: "Survex" }
                                    QC.MenuItem { text: "Compass" }
                                }
                            }
                        }

                        QC.Label {
                            id: tipLabelId
                            text: "Hover for a tip"
                            // Shown only while the label and the tip above it lie
                            // inside the viewport; a tip on the overlay ignores clipping.
                            QC.ToolTip.visible: {
                                const flick = galleryScrollViewId.contentItem as QQ.Flickable
                                // Reading the flow's height re-runs this as cards reflow.
                                const y = tipLabelId.mapToItem(cardFlowId, 0, 0).y + 0 * cardFlowId.height
                                return y - tipLabelId.height >= flick.contentY
                                        && y + tipLabelId.height <= flick.contentY + flick.height
                            }
                            QC.ToolTip.text: "A one-line tip"
                        }
                    }
                }

                GalleryCard {
                    objectName: "cardContainers"
                    title: "Containers"

                    QC.GroupBox {
                        Layout.fillWidth: true
                        title: "Group"

                        QC.Label { text: "Inside a titled group box" }
                    }

                    QC.Frame {
                        Layout.fillWidth: true

                        QC.Label { text: "Inside a frame" }
                    }

                    QC.ScrollView {
                        Layout.fillWidth: true
                        Layout.preferredHeight: galleryPage.scrollDemoHeight
                        contentWidth: availableWidth
                        QC.ScrollBar.vertical.policy: QC.ScrollBar.AlwaysOn

                        ColumnLayout {
                            width: parent.width
                            height: galleryPage.scrollDemoContentHeight

                            QQ.Repeater {
                                model: 8
                                delegate: QC.Label {
                                    required property int index
                                    text: "Scrolled row " + (index + 1)
                                }
                            }
                        }
                    }

                    QC.SplitView {
                        Layout.fillWidth: true
                        Layout.preferredHeight: galleryPage.splitDemoHeight

                        QQ.Rectangle {
                            QC.SplitView.preferredWidth: galleryPage.cardWidth / 2
                            color: Theme.surfaceMuted

                            QC.Label {
                                anchors.centerIn: parent
                                text: "Left pane"
                            }
                        }

                        QQ.Rectangle {
                            QC.SplitView.fillWidth: true
                            color: Theme.surfaceRaised

                            QC.Label {
                                anchors.centerIn: parent
                                text: "Right pane"
                            }
                        }
                    }
                }

                GalleryCard {
                    objectName: "cardData"
                    title: "Data"

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 0

                        QQ.Repeater {
                            model: ["Entrance", "Junction room", "Canyon", "Breakdown", "Sump"]
                            delegate: QC.ItemDelegate {
                                required property string modelData
                                required property int index
                                Layout.fillWidth: true
                                text: modelData
                                highlighted: index === 1
                            }
                        }
                    }

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 0

                        QQ.Repeater {
                            model: 6
                            delegate: TableRowBackground {
                                id: rowId
                                required property int index
                                Layout.fillWidth: true
                                implicitHeight: rowLabelId.implicitHeight + 2 * Theme.delegatePadding
                                rowIndex: index
                                isSelected: index === 2

                                QC.Label {
                                    id: rowLabelId
                                    anchors.verticalCenter: parent.verticalCenter
                                    x: Theme.delegatePadding
                                    text: "A" + (rowId.index + 1) + "  →  A" + (rowId.index + 2)
                                }
                            }
                        }
                    }

                    LinkText { text: "Open the manual" }

                    ClickTextInput {
                        text: "12.5 m"
                    }
                }

                GalleryCard {
                    objectName: "cardDialogs"
                    title: "Dialogs"

                    QC.Button {
                        objectName: "galleryDialogButton"
                        text: "Remove note…"
                        onClicked: galleryPage.openDialog()
                    }

                    QQ.Rectangle {
                        Layout.fillWidth: true
                        implicitHeight: warningLabelId.implicitHeight + 2 * Theme.statsPadding
                        color: Theme.warningSurface
                        border.width: 1
                        border.color: Theme.warningBorder
                        radius: Theme.floatingWidgetRadius

                        QC.Label {
                            id: warningLabelId
                            anchors.left: parent.left
                            anchors.right: parent.right
                            anchors.verticalCenter: parent.verticalCenter
                            anchors.margins: Theme.statsPadding
                            color: Theme.warningText
                            wrapMode: QC.Label.WordWrap
                            text: "This trip has stations without a fix."
                        }
                    }

                    HelpArea {
                        Layout.fillWidth: true
                        visible: true
                        imageSource: "qrc:/twbs-icons/icons/question-circle.svg"
                        text: "Help text explains what a field means."
                    }
                }

                GalleryCard {
                    objectName: "cardCaveWhere"
                    title: "CaveWhere widgets"

                    RowLayout {
                        spacing: Theme.columnGap

                        IconButton {
                            iconSource: "qrc:/twbs-icons/icons/layers.svg"
                            sourceSize: Qt.size(Theme.iconSizeSmall, Theme.iconSizeSmall)
                            text: "Layers"
                        }

                        InformationButton {
                            showItemOnClick: labelHelpId
                        }

                        LabelWithHelp {
                            text: "Label with help"
                            helpArea: labelHelpId
                        }
                    }

                    HelpArea {
                        id: labelHelpId
                        Layout.fillWidth: true
                        text: "Shown by the label or the information button."
                    }

                    GroupBox {
                        Layout.fillWidth: true
                        text: "Legacy group box"
                        backgroundColor: Theme.surface
                        contentHeight: galleryPage.legacyGroupContentHeight

                        QC.Label {
                            anchors.centerIn: parent
                            text: "Legacy content"
                        }
                    }

                    CheckableGroupBox {
                        text: "Checkable group box"
                        checked: true
                        backgroundColor: Theme.surface

                        QC.Label { text: "Checkable content" }
                    }

                    QQ.Item {
                        Layout.fillWidth: true
                        implicitHeight: quoteLabelId.implicitHeight + galleryPage.quoteTipHeight

                        QuoteBox {
                            x: galleryPage.quoteTipInset

                            QC.Label {
                                id: quoteLabelId
                                text: "A quote box points at its owner"
                            }
                        }
                    }
                }
            }
        }
    }

    QC.Dialog {
        id: removeDialogId
        objectName: "galleryDialog"
        popupType: QC.Popup.Item
        parent: QC.Overlay.overlay
        anchors.centerIn: parent
        modal: true
        title: "Remove this note?"
        standardButtons: QC.Dialog.Ok | QC.Dialog.Cancel

        QC.Label { text: "The note and its scraps leave the trip." }
    }
}
