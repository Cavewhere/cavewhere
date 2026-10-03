import QtQuick
import QtQuick.Controls as QC
import QtQuick.Layouts
import QtTest
import cavewherelib

MainWindowTest {
    id: rootId

    SectionGroupBox {
        id: titledBoxId
        width: 300
        title: "Project"

        ColumnLayout {
            QC.Label { text: "Row one" }
            QC.Label { text: "Row two" }
        }
    }

    SectionGroupBox {
        id: untitledBoxId
        y: 200
        width: 300

        QC.Label { text: "Untitled" }
    }

    TestCase {
        name: "SectionGroupBox"
        when: windowShown

        function cleanup() {
            RootData.settings.appearanceSettings.colorScheme = AppearanceSettings.System
        }

        function test_titleIsBoldAtTopPadding() {
            const label = titledBoxId.label
            compare(label.text, "Project")
            verify(label.font.bold)
            compare(label.y, titledBoxId.verticalPadding)
            compare(titledBoxId.verticalPadding, Theme.statsPadding)
            verify(titledBoxId.topPadding > titledBoxId.bottomPadding)
        }

        function test_untitledReservesNoTitleRoom() {
            compare(untitledBoxId.topPadding, untitledBoxId.bottomPadding)
        }

        function checkBackground() {
            const background = titledBoxId.background
            compare(background.radius, Theme.panelRadius)
            compare(background.border.width, 0)
            verify(Qt.colorEqual(background.color, Theme.sectionFill))
        }

        function test_backgroundInLightAndDark() {
            RootData.settings.appearanceSettings.colorScheme = AppearanceSettings.Light
            tryVerify(() => !Theme.dark)
            checkBackground()
            verify(Qt.colorEqual(titledBoxId.background.color, "#E0E0E0"))

            RootData.settings.appearanceSettings.colorScheme = AppearanceSettings.Dark
            tryVerify(() => Theme.dark)
            checkBackground()
            verify(Qt.colorEqual(titledBoxId.background.color, "#424242"))
        }
    }
}
