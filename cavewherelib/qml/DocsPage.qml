import QtQuick as QQ
import QtQuick.Controls as QC
import cavewherelib

// In-app user-manual viewer. CP1 renders a single hard-coded Markdown string
// through Qt's native MarkdownText to prove the render path, theming, and that
// the page shows through cwPageView. CP2 replaces the placeholder by loading a
// per-article Markdown body selected by a slug property.
StandardPage {
    id: docsPageId

    // The reading column never grows past a comfortable measure; below that it
    // tracks the viewport so narrow windows still fill the width.
    readonly property int readingMaxWidth: 720
    readonly property int readingPadding: 32

    property string markdownText: docsPageId.placeholderMarkdown

    readonly property string placeholderMarkdown: [
        "# Docs viewer preview",
        "",
        "This page is a **CP1 placeholder**. It renders through Qt's native",
        "`MarkdownText`, with no web engine, and shows through the same",
        "`cwPageView` every other page uses — so back, forward, and the",
        "breadcrumb already work. Real per-article content arrives in CP2.",
        "",
        "## What this proves",
        "",
        "- Markdown renders natively and picks up the app's theme colors and font.",
        "- The page is reachable from **File → Docs** and lives in page history.",
        "- Links, tables, and code all come through Qt's renderer:",
        "",
        "| Construct | Renders |",
        "| --- | --- |",
        "| Headings, bold, lists | Yes |",
        "| Tables | Yes |",
        "| Inline `code` and blocks | Yes |",
        "",
        "> The article body uses Qt's default Markdown styling; the surrounding",
        "> chrome — sidebar, search, header — is rebuilt natively in later checkpoints.",
        "",
        "For the full manual, see [cavewhere.com](https://cavewhere.com).",
    ].join("\n")

    QQ.Rectangle {
        anchors.fill: parent
        color: Theme.background
    }

    QC.ScrollView {
        id: scrollId
        anchors.fill: parent
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

                // CP3 routes .md links through the page model; until then only
                // external links are meaningful, so hand them to the browser.
                onLinkActivated: function(link) {
                    Qt.openUrlExternally(link)
                }
            }
        }
    }
}
