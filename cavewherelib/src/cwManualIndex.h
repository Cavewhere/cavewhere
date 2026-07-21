#ifndef CWMANUALINDEX_H
#define CWMANUALINDEX_H

//Qt includes
#include <QObject>
#include <QString>
#include <QList>
#include <QHash>
#include <QVariantList>

//Our includes
#include "CaveWhereLibExport.h"

/**
 * @brief In-memory index of the embedded user manual for the in-app Docs viewer.
 *
 * The manual ships as a Qt resource under ":/manual" (see the qt_add_resources
 * step in cavewherelib/CMakeLists.txt). Resources are memory-mapped and
 * demand-paged, so nothing is read until asked for: construction parses only the
 * small llms.txt index (order, title, path); each article body — and the images
 * it references — is read on demand.
 *
 * Article bodies are served front-matter-stripped and with their relative image
 * links rewritten to absolute qrc: URLs, because Qt's Text/Label has no settable
 * Markdown base URL.
 */
class CAVEWHERE_LIB_EXPORT cwManualIndex : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QVariantList articles READ articles CONSTANT)

public:
    struct Article {
        QString slug;          //!< stable id, e.g. "scraps-scraps-and-carpeting"
        QString title;         //!< human title from llms.txt
        QString chapter;       //!< sidebar grouping derived from the path
        QString relativePath;  //!< e.g. "scraps/scraps-and-carpeting.md"
        QString summary;       //!< one-line description from llms.txt (feeds search)
    };

    explicit cwManualIndex(QObject* parent = nullptr);

    QVariantList articles() const;
    const QList<Article>& articleList() const;

    //Renders a manual article to display-ready Markdown. An empty @p slug is the
    //"Docs" landing page (the manual index), so the viewer can bind one call to
    //its slug without branching on the empty case itself.
    Q_INVOKABLE QString body(const QString& slug) const;
    Q_INVOKABLE QString landingBody() const;

    //Classifies a Markdown link clicked on the page whose slug is @p fromSlug
    //(empty for the landing page) so the viewer knows exactly how to follow it,
    //rather than guessing from the raw href. The returned map always carries a
    //"kind":
    //  "anchor"   — a same-page "#heading" link; also carries "anchor" (the raw
    //               "#..." fragment) for the viewer to scroll to.
    //  "article"  — an in-app manual article; also carries "slug".
    //  "external" — a scheme link (http, mailto, …) or one resolving outside the
    //               manual; the viewer opens it in the browser.
    //  "missing"  — resolves inside the manual but is not a registered article
    //               (a dead in-manual link); the viewer ignores it rather than
    //               handing a bare relative path to the browser.
    Q_INVOKABLE QVariantMap resolveLink(const QString& fromSlug, const QString& href) const;

    //Lowercased "title summary keywords" haystack for the article @p slug, used
    //by the search filter. The title and summary come from llms.txt (parsed at
    //construction); the keywords are read from the article's front matter on
    //first use and cached, so nothing extra is touched until a search runs.
    Q_INVOKABLE QString searchText(const QString& slug) const;

    static QString slugForPath(const QString& relativePath);
    static QString chapterForPath(const QString& relativePath);

private:
    QString renderMarkdown(const QString& relativePath) const;
    static QString stripFrontMatter(const QString& text);
    static QString rewriteImageLinks(const QString& body, const QString& relativePath);
    static QString frontMatterKeywords(const QString& relativePath);

    QList<Article> m_articles;
    QHash<QString, int> m_slugToIndex;
    mutable QHash<QString, QString> m_searchTextCache;
};

#endif // CWMANUALINDEX_H
