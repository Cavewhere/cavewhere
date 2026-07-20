import QtQuick as QQ
import QtQuick.Window as QQ
import QtQuick.Controls as QC
import cavewherelib

// In-app user-manual viewer. One shared component drives every article: the
// slug selects the page, and cwManualIndex resolves it to the front-matter-
// stripped Markdown body (with image links rewritten to absolute qrc: URLs).
// An empty slug is the "Docs" landing page, which renders the manual's index.
//
// The body is a read-only, document-backed text view so the reader can find
// text within the page (Ctrl+F) and jump to a heading through a same-page
// "#anchor" link — cwDocumentFinder does both over the rendered QTextDocument.
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

    // A found match or an anchor target is scrolled this far below the viewport
    // top, so it lands in a readable spot rather than glued to the edge.
    readonly property int scrollTopMargin: 24

    // Find-on-page bar geometry.
    readonly property int findBarPadding: 8
    readonly property int findBarSpacing: 6
    readonly property int findCountWidth: 52

    // Below this the persistent sidebar would starve the reading column, so it
    // collapses into a drawer (the same breakpoint the other pages collapse at).
    readonly property bool isNarrow: width < Theme.breakpointPanelCollapse

    // Margin shared by the top-row affordances (drawer toggle, find button/bar).
    readonly property int topBarMargin: 8

    // The reading column's top clears the compact-mode drawer-toggle row. In
    // compact mode the find bar shares that row, so it needs no extra inset; in
    // wide mode the find bar reserves its own band above the column instead.
    readonly property int topInset: docsPageId.isNarrow
                                    ? sidebarToggleId.height + 2 * docsPageId.topBarMargin
                                    : 0

    // The Docs landing page — the parent under which every article registers as
    // a child keyed by its slug (see MainContent). Injected for article pages;
    // the landing page falls back to its own hosting page, and the standalone
    // test sets it explicitly.
    property Page docsRootPage: docsPageId.PageView.page

    property string slug: ""

    property bool findVisible: false

    // An empty slug is the landing page; cwManualIndex.body() handles that case,
    // so this is a single unconditional call.
    property string markdownText: RootData.manualIndex.body(docsPageId.slug)

    // Navigate to a manual article by its stable slug. Each article is a child
    // of the Docs page registered under its slug, so this is one ordinary
    // history navigation to that child — no address strings, no title round-trip.
    function navigateToSlug(slug: string) {
        RootData.pageSelectionModel.gotoPageByName(docsPageId.docsRootPage, slug)
    }

    // The reader picked an article from the sidebar: dismiss the drawer (a no-op
    // when it is the pinned panel) and navigate.
    function selectSlug(slug: string) {
        sidebarDrawerId.close()
        docsPageId.navigateToSlug(slug)
    }

    // Scroll the reading column so the character at @p position sits just below
    // the top of the viewport (clamped to the scrollable range).
    function scrollToDocumentPosition(position: int) {
        let rect = bodyId.positionToRectangle(position)
        let target = bodyId.y + rect.y - docsPageId.scrollTopMargin
        let maxY = Math.max(0, scrollId.contentHeight - scrollId.height)
        scrollId.contentY = Math.max(0, Math.min(target, maxY))
    }

    function openFind() {
        docsPageId.findVisible = true
        findFieldId.forceActiveFocus()
        findFieldId.selectAll()
    }

    function closeFind() {
        docsPageId.findVisible = false
        bodyId.deselect()
    }

    // Growing back to the pinned layout while the drawer is open would leave an
    // empty modal popup over the reading column, so dismiss it on the way out.
    onIsNarrowChanged: {
        if (!docsPageId.isNarrow) {
            sidebarDrawerId.close()
        }
    }

    // A new article invalidates the current find state and its matches.
    onSlugChanged: docsPageId.closeFind()

    // Ctrl+F (Cmd+F on macOS) opens the find bar and focuses it.
    QQ.Shortcut {
        id: findShortcutId
        sequence: QQ.StandardKey.Find
        enabled: docsPageId.visible
        onActivated: docsPageId.openFind()
    }

    // Step through matches while the bar is open: F3 / Cmd+G (next) and
    // Shift+F3 / Cmd+Shift+G (previous), the platform-standard find keys.
    QQ.Shortcut {
        id: findNextShortcutId
        sequence: QQ.StandardKey.FindNext
        enabled: docsPageId.visible && docsPageId.findVisible
        onActivated: finderId.findNext()
    }

    QQ.Shortcut {
        id: findPreviousShortcutId
        sequence: QQ.StandardKey.FindPrevious
        enabled: docsPageId.visible && docsPageId.findVisible
        onActivated: finderId.findPrevious()
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

    // Locates matches and heading anchors in the rendered body document, so the
    // find bar can step through matches and "#anchor" links can scroll to their
    // heading. The query is empty while the bar is closed, which clears matches.
    DocumentFinder {
        id: finderId
        objectName: "docsFinder"

        document: bodyId.textDocument
        query: docsPageId.findVisible ? findFieldId.text : ""

        onMatchSelected: (position, length) => {
            bodyId.select(position, position + length)
            docsPageId.scrollToDocumentPosition(position)
        }
        onMatchCountChanged: {
            if (finderId.matchCount === 0) {
                bodyId.deselect()
            }
        }
    }

    // Styles the rendered Markdown to match the web manual: heading sizes, block
    // spacing, and — what no stylesheet can do for Markdown — images clamped to the
    // reading column so figures shrink instead of overflowing on a narrow window.
    DocumentStyler {
        id: stylerId
        objectName: "docsStyler"

        document: bodyId.textDocument
        baseFontSize: bodyId.font.pixelSize
        contentWidth: bodyId.width
        mutedColor: Theme.textSubtle
        linkColor: Theme.textLink
        headingFontFamily: Theme.fontFamilyHeading
    }

    QQ.Flickable {
        id: scrollId

        anchors.left: docsPageId.isNarrow ? parent.left : sidebarPanelId.right
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        // Wide mode always reserves a top band the height of the find bar: it
        // holds the search button when find is closed and the bar when it is
        // open, so neither ever covers text and there is no shift on open/close.
        // Compact mode reserves that row via topInset (the drawer-toggle row).
        anchors.topMargin: docsPageId.topInset
                           + (docsPageId.isNarrow ? 0 : findBarId.height)

        contentWidth: width
        contentHeight: bodyWrapId.height
        boundsBehavior: QQ.Flickable.StopAtBounds
        clip: true

        QC.ScrollBar.vertical: QC.ScrollBar { }

        QQ.Item {
            id: bodyWrapId

            width: scrollId.width
            height: bodyId.implicitHeight + 2 * docsPageId.readingPadding

            QC.TextArea {
                id: bodyId
                objectName: "docsBody"

                x: Math.max(docsPageId.readingPadding, (parent.width - width) / 2)
                y: docsPageId.readingPadding
                width: Math.min(docsPageId.readingMaxWidth,
                                parent.width - 2 * docsPageId.readingPadding)

                text: docsPageId.markdownText
                textFormat: QC.TextArea.MarkdownText
                wrapMode: QC.TextArea.Wrap
                readOnly: true
                padding: 0
                background: null
                color: Theme.text
                selectionColor: Theme.highlight
                selectedTextColor: Theme.textInverse
                font.family: Theme.fontFamilyReading
                font.pixelSize: Theme.fontSizeUI

                // cwManualIndex classifies the link so the viewer never has to
                // guess from the raw href: a same-page anchor scrolls to its
                // heading, an in-app article navigates through the page model,
                // an external link opens in the browser, and a dead in-manual
                // link is ignored rather than handed to the browser.
                onLinkActivated: function(link) {
                    let action = RootData.manualIndex.resolveLink(docsPageId.slug, link)
                    switch (action.kind) {
                    case "anchor": {
                        let position = finderId.headingPosition(action.anchor)
                        if (position >= 0) {
                            docsPageId.scrollToDocumentPosition(position)
                        }
                        break
                    }
                    case "article":
                        docsPageId.navigateToSlug(action.slug)
                        break
                    case "external":
                        Qt.openUrlExternally(link)
                        break
                    }
                }

                // A pointing-hand cursor marks links as clickable. Over plain
                // text the read-only editor keeps its selection I-beam, since
                // the body is still selectable for copy. hoveredLink stays
                // current because this handler doesn't block the TextArea's
                // own hover events (blocking defaults to false).
                QQ.HoverHandler {
                    objectName: "docsBodyCursor"
                    cursorShape: bodyId.hoveredLink !== ""
                                 ? Qt.PointingHandCursor
                                 : Qt.IBeamCursor
                }
            }
        }
    }

    // Find-on-page bar. Wide: a full-width toolbar docked in a reserved band
    // above the reading column (the column's top margin grows to make room), so
    // it never covers any text. Compact: it shares the drawer toggle's row,
    // starting just right of the toggle, so no extra vertical space is used. The
    // query field stretches; the count and buttons sit right.
    QQ.Rectangle {
        id: findBarId

        anchors.left: docsPageId.isNarrow ? sidebarToggleId.right : scrollId.left
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.leftMargin: docsPageId.isNarrow ? docsPageId.topBarMargin : 0
        anchors.rightMargin: docsPageId.isNarrow ? docsPageId.topBarMargin : 0
        anchors.topMargin: docsPageId.isNarrow ? docsPageId.topBarMargin : 0

        height: docsPageId.isNarrow
                ? sidebarToggleId.height
                : findFieldId.implicitHeight + 2 * docsPageId.findBarPadding
        color: Theme.surfaceRaised

        visible: docsPageId.findVisible

        // Separator line between the bar and the document just below it.
        QQ.Rectangle {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            height: 1
            color: Theme.border
        }

        QC.ToolButton {
            id: findCloseId
            objectName: "docsFindClose"

            anchors.right: parent.right
            anchors.verticalCenter: parent.verticalCenter
            anchors.rightMargin: docsPageId.findBarPadding

            icon.source: "qrc:/twbs-icons/icons/x.svg"
            icon.color: Theme.text

            onClicked: docsPageId.closeFind()
        }

        QC.ToolButton {
            id: findNextId
            objectName: "docsFindNext"

            anchors.right: findCloseId.left
            anchors.verticalCenter: parent.verticalCenter

            enabled: finderId.matchCount > 0
            icon.source: "qrc:/twbs-icons/icons/chevron-down.svg"
            icon.color: Theme.text

            QC.ToolTip.text: qsTr("Next match") + " (" + findNextShortcutId.nativeText + ")"
            QC.ToolTip.delay: 500
            QC.ToolTip.visible: hovered

            onClicked: finderId.findNext()
        }

        QC.ToolButton {
            id: findPreviousId
            objectName: "docsFindPrevious"

            anchors.right: findNextId.left
            anchors.verticalCenter: parent.verticalCenter

            enabled: finderId.matchCount > 0
            icon.source: "qrc:/twbs-icons/icons/chevron-up.svg"
            icon.color: Theme.text

            QC.ToolTip.text: qsTr("Previous match") + " (" + findPreviousShortcutId.nativeText + ")"
            QC.ToolTip.delay: 500
            QC.ToolTip.visible: hovered

            onClicked: finderId.findPrevious()
        }

        QC.Label {
            id: findStatusId
            objectName: "docsFindStatus"

            anchors.right: findPreviousId.left
            anchors.verticalCenter: parent.verticalCenter
            width: docsPageId.findCountWidth
            horizontalAlignment: QC.Label.AlignHCenter

            text: findFieldId.text.length === 0
                  ? ""
                  : (finderId.matchCount === 0
                     ? qsTr("None")
                     : finderId.currentMatch + "/" + finderId.matchCount)
            color: Theme.textSubtle
            font.family: Theme.fontFamilyBody
            font.pixelSize: Theme.fontSizeSmall
        }

        QC.TextField {
            id: findFieldId
            objectName: "docsFindField"

            anchors.left: parent.left
            anchors.right: findStatusId.left
            anchors.verticalCenter: parent.verticalCenter
            anchors.leftMargin: docsPageId.findBarPadding
            anchors.rightMargin: docsPageId.findBarSpacing

            placeholderText: qsTr("Find in page")
            font.family: Theme.fontFamilyBody
            font.pixelSize: Theme.fontSizeBody

            onAccepted: finderId.findNext()
            QQ.Keys.onEscapePressed: docsPageId.closeFind()

            // Shift+Enter steps backward; plain Enter falls through to onAccepted.
            QQ.Keys.onReturnPressed: (event) => {
                if (event.modifiers & Qt.ShiftModifier) {
                    finderId.findPrevious()
                    event.accepted = true
                } else {
                    event.accepted = false
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
        anchors.margins: docsPageId.topBarMargin

        visible: docsPageId.isNarrow
        icon.source: "qrc:/twbs-icons/icons/list.svg"
        icon.color: Theme.text

        onClicked: sidebarDrawerId.open()
    }

    // The affordance that opens the find bar — hidden while the bar is showing.
    // Top-left of the reading column (right of the sidebar), centered in the
    // reserved top band; sits just right of the drawer toggle in compact mode.
    QC.RoundButton {
        id: findButtonId
        objectName: "docsFindButton"

        anchors.left: docsPageId.isNarrow ? sidebarToggleId.right : scrollId.left
        anchors.leftMargin: docsPageId.topBarMargin
        anchors.verticalCenter: findBarId.verticalCenter

        visible: !docsPageId.findVisible
        icon.source: "qrc:/twbs-icons/icons/search.svg"
        icon.color: Theme.text

        QC.ToolTip.text: qsTr("Find in page") + " (" + findShortcutId.nativeText + ")"
        QC.ToolTip.delay: 500
        QC.ToolTip.visible: hovered

        onClicked: docsPageId.openFind()
    }
}
