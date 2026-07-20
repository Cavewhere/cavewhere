import QtQuick as QQ
import QtQuick.Window as QQ
import QtQuick.Controls as QC
import cavewherelib

// In-app user-manual viewer. One shared component drives every article: the
// slug selects the page, and cwManualIndex resolves it to the front-matter-
// stripped Markdown body (with image links rewritten to absolute qrc: URLs).
// An empty slug is the "Docs" landing page, which renders the manual's index.
//
// The navigation surface (DocsSidebar: search + chapter-grouped TOC) adapts to
// width: a persistent left panel on wide windows, and a slide-in Drawer reached
// by a toolbar button when the window is too narrow to spare the room.
StandardPage {
    id: docsPageId

    // The reading column never grows past a comfortable measure; below that it
    // tracks the viewport so narrow windows still fill the width.
    readonly property int readingMaxWidth: 720
    readonly property int readingPadding: 32
    readonly property int sidebarWidth: 260

    // Below this the persistent sidebar would starve the reading column, so it
    // collapses into a drawer (the same breakpoint the other pages collapse at).
    readonly property bool isNarrow: width < Theme.breakpointPanelCollapse

    // Top-level page under which every article is registered (see MainContent);
    // article addresses are "Docs/<article title>".
    readonly property string docsRootName: "Docs"

    property string slug: ""

    property string markdownText: docsPageId.slug === ""
                                  ? RootData.manualIndex.landingBody()
                                  : RootData.manualIndex.body(docsPageId.slug)

    // Navigate to a manual article by its stable slug. Articles register under
    // their human title, so the address is "Docs/<title>"; going through
    // currentPageAddress makes each hop an ordinary history navigation.
    function navigateToSlug(slug: string) {
        RootData.pageSelectionModel.currentPageAddress =
            docsPageId.docsRootName + "/" + RootData.manualIndex.title(slug)
    }

    // The reader picked an article from the sidebar: dismiss the drawer (a no-op
    // when it is the pinned panel) and navigate.
    function selectSlug(slug: string) {
        sidebarDrawerId.close()
        docsPageId.navigateToSlug(slug)
    }

    // Growing back to the pinned layout while the drawer is open would leave an
    // empty modal popup over the reading column, so dismiss it on the way out.
    onIsNarrowChanged: {
        if (!docsPageId.isNarrow) {
            sidebarDrawerId.close()
        }
    }

    // One sidebar definition, hosted by whichever container the width calls for.
    QQ.Component {
        id: sidebarComponentId

        DocsSidebar {
            currentSlug: docsPageId.slug
            onNavigate: (slug) => docsPageId.selectSlug(slug)
        }
    }

    QQ.Rectangle {
        anchors.fill: parent
        color: Theme.background
    }

    // Wide: the sidebar is pinned to the left and the reading column is inset.
    QQ.Loader {
        id: sidebarPanelId

        anchors.left: parent.left
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        width: docsPageId.sidebarWidth

        active: !docsPageId.isNarrow
        visible: active
        sourceComponent: sidebarComponentId
    }

    QQ.Rectangle {
        anchors.left: sidebarPanelId.right
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        width: 1
        color: Theme.border
        visible: sidebarPanelId.active
    }

    // Narrow: the sidebar slides in over the reading column.
    QC.Drawer {
        id: sidebarDrawerId
        objectName: "docsSidebarDrawer"

        edge: Qt.LeftEdge
        width: docsPageId.sidebarWidth
        height: docsPageId.QQ.Window.window ? docsPageId.QQ.Window.window.height
                                            : docsPageId.height
        interactive: docsPageId.isNarrow

        QQ.Loader {
            anchors.fill: parent
            active: docsPageId.isNarrow
            sourceComponent: sidebarComponentId
        }
    }

    QC.ScrollView {
        id: scrollId

        anchors.left: docsPageId.isNarrow ? parent.left : sidebarPanelId.right
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        anchors.topMargin: docsPageId.isNarrow ? sidebarToggleId.height + 16 : 0
        contentWidth: availableWidth

        QQ.Item {
            implicitWidth: scrollId.availableWidth
            implicitHeight: bodyId.implicitHeight + 2 * docsPageId.readingPadding

            QC.Label {
                id: bodyId
                objectName: "docsBody"

                x: Math.max(docsPageId.readingPadding, (parent.width - width) / 2)
                y: docsPageId.readingPadding
                width: Math.min(docsPageId.readingMaxWidth,
                                parent.width - 2 * docsPageId.readingPadding)

                text: docsPageId.markdownText
                textFormat: QC.Label.MarkdownText
                wrapMode: QC.Label.Wrap
                color: Theme.text
                linkColor: Theme.textLink
                font.family: Theme.fontFamilyBody
                font.pixelSize: Theme.fontSizeBody

                // Relative .md links (the landing index and inter-article
                // cross-references) navigate in-app through the page model;
                // everything else (http(s), mailto) opens in the browser.
                // Same-page anchors are ignored for now — intra-page scrolling
                // is a fast-follow.
                onLinkActivated: function(link) {
                    if (link.startsWith("#")) {
                        return
                    }

                    let targetSlug = RootData.manualIndex.slugForLink(docsPageId.slug, link)
                    if (targetSlug.length > 0) {
                        docsPageId.navigateToSlug(targetSlug)
                    } else {
                        Qt.openUrlExternally(link)
                    }
                }
            }
        }
    }

    // The affordance that opens the drawer — only when the sidebar is collapsed.
    QC.RoundButton {
        id: sidebarToggleId
        objectName: "docsSidebarToggle"

        anchors.left: parent.left
        anchors.top: parent.top
        anchors.margins: 8

        visible: docsPageId.isNarrow
        icon.source: "qrc:/twbs-icons/icons/list.svg"
        icon.color: Theme.text

        onClicked: sidebarDrawerId.open()
    }
}
