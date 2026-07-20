import QtQuick as QQ
import QtQuick.Controls as QC
import cavewherelib

// The manual's navigation surface: a search box over a chapter-grouped table of
// contents (a ManualSearchModel over RootData.manualIndex). DocsPage hosts this
// either as its persistent left panel (wide windows) or inside a Drawer (compact
// windows); it emits navigate() when the reader picks an article so the host can
// route to that page and, in the drawer case, dismiss itself.
QQ.Rectangle {
    id: sidebarId

    property string currentSlug: ""

    color: Theme.surfaceMuted

    signal navigate(slug: string)

    ManualSearchModel {
        id: searchModelId
        objectName: "docsSearchModel"
        manualIndex: RootData.manualIndex
        query: searchFieldId.text
    }

    QC.TextField {
        id: searchFieldId
        objectName: "docsSearchField"

        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.margins: 10

        placeholderText: qsTr("Search the manual")
        font.family: Theme.fontFamilyBody
        font.pixelSize: Theme.fontSizeBody
    }

    QC.ScrollView {
        id: tocScrollId

        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: searchFieldId.bottom
        anchors.bottom: parent.bottom
        anchors.topMargin: 6
        contentWidth: availableWidth
        clip: true

        QQ.ListView {
            id: tocListId
            objectName: "docsTocList"

            model: searchModelId
            boundsBehavior: QQ.Flickable.StopAtBounds

            section.property: "chapter"
            section.criteria: QQ.ViewSection.FullString
            section.delegate: QQ.Item {
                required property string section

                width: tocListId.width
                implicitHeight: sectionLabelId.implicitHeight + 14

                QC.Label {
                    id: sectionLabelId
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.bottom: parent.bottom
                    anchors.leftMargin: 12
                    anchors.rightMargin: 12
                    anchors.bottomMargin: 3

                    text: parent.section
                    color: Theme.textSubtle
                    font.family: Theme.fontFamilyBody
                    font.pixelSize: Theme.fontSizeCaption
                    font.capitalization: QQ.Font.AllUppercase
                    font.bold: true
                    elide: QC.Label.ElideRight
                }
            }

            delegate: QQ.Rectangle {
                id: tocDelegateId

                required property string slug
                required property string title

                readonly property bool current: tocDelegateId.slug === sidebarId.currentSlug

                width: tocListId.width
                implicitHeight: entryLabelId.implicitHeight + 12
                color: tocDelegateId.current ? Theme.highlight
                                             : (hoverHandlerId.hovered ? Theme.hover : "transparent")

                QC.Label {
                    id: entryLabelId
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.verticalCenter: parent.verticalCenter
                    anchors.leftMargin: 18
                    anchors.rightMargin: 12

                    text: tocDelegateId.title
                    color: tocDelegateId.current ? Theme.textInverse : Theme.text
                    font.family: Theme.fontFamilyBody
                    font.pixelSize: Theme.fontSizeBody
                    wrapMode: QC.Label.Wrap
                }

                QQ.HoverHandler {
                    id: hoverHandlerId
                    cursorShape: Qt.PointingHandCursor
                }

                QQ.TapHandler {
                    onTapped: sidebarId.navigate(tocDelegateId.slug)
                }
            }
        }
    }
}
