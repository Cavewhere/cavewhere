import QtQuick as QQ
import QtQuick.Controls as QC
import cavewherelib

// In-app user-manual viewer. One shared component drives every article: the
// slug selects the page, and cwManualIndex resolves it to the front-matter-
// stripped Markdown body (with image links rewritten to absolute qrc: URLs).
// An empty slug is the "Docs" landing page, which renders the manual's index.
StandardPage {
    id: docsPageId

    // The reading column never grows past a comfortable measure; below that it
    // tracks the viewport so narrow windows still fill the width.
    readonly property int readingMaxWidth: 720
    readonly property int readingPadding: 32

    // Top-level page under which every article is registered (see MainContent);
    // article addresses are "Docs/<article title>".
    readonly property string docsRootName: "Docs"

    property string slug: ""

    property string markdownText: docsPageId.slug === ""
                                  ? RootData.manualIndex.landingBody()
                                  : RootData.manualIndex.body(docsPageId.slug)

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
                        RootData.pageSelectionModel.currentPageAddress =
                            docsPageId.docsRootName + "/" + RootData.manualIndex.title(targetSlug)
                    } else {
                        Qt.openUrlExternally(link)
                    }
                }
            }
        }
    }
}
