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
    };

    explicit cwManualIndex(QObject* parent = nullptr);

    QVariantList articles() const;
    const QList<Article>& articleList() const;

    Q_INVOKABLE QString body(const QString& slug) const;
    Q_INVOKABLE QString landingBody() const;
    Q_INVOKABLE QString title(const QString& slug) const;

    static QString slugForPath(const QString& relativePath);
    static QString chapterForPath(const QString& relativePath);

private:
    QString renderMarkdown(const QString& relativePath) const;
    static QString stripFrontMatter(const QString& text);
    static QString rewriteImageLinks(const QString& body, const QString& relativePath);

    QList<Article> m_articles;
    QHash<QString, int> m_slugToIndex;
};

#endif // CWMANUALINDEX_H
