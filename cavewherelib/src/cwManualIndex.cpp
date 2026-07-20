//Our includes
#include "cwManualIndex.h"

//Qt includes
#include <QFile>
#include <QTextStream>
#include <QRegularExpression>
#include <QUrl>
#include <QVariantMap>
#include <QDebug>

namespace {
    const QString kResourceRoot = QStringLiteral(":/manual/");
    const QString kResourceUrlRoot = QStringLiteral("qrc:/manual/");
    const QString kMarkdownSuffix = QStringLiteral(".md");
    const QString kLandingPage = QStringLiteral("index.md");
    const QString kLlmsIndex = QStringLiteral("llms.txt");

    //Title-case a chapter directory the way Python's str.title() does, so the
    //grouping labels match the website build (e.g. "point-clouds" -> "Point Clouds").
    QString titleCase(QString text)
    {
        text.replace(QLatin1Char('-'), QLatin1Char(' '));
        bool startOfWord = true;
        for (int i = 0; i < text.size(); ++i) {
            if (text.at(i).isSpace()) {
                startOfWord = true;
            } else {
                text[i] = startOfWord ? text.at(i).toUpper() : text.at(i).toLower();
                startOfWord = false;
            }
        }
        return text;
    }

    //Resolve a Markdown image URL against the article's resource directory,
    //leaving anything already absolute (a scheme, a root path, or a bare anchor)
    //untouched.
    QString resolveImageUrl(const QString& url, const QString& baseUrl)
    {
        if (url.startsWith(QLatin1Char('#')) || url.startsWith(QLatin1Char('/'))) {
            return url;
        }
        const QUrl parsed(url);
        if (!parsed.scheme().isEmpty()) {
            return url;
        }
        return QUrl(baseUrl).resolved(parsed).toString();
    }
}

cwManualIndex::cwManualIndex(QObject* parent) :
    QObject(parent)
{
    QFile file(kResourceRoot + kLlmsIndex);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qWarning() << "cwManualIndex: " << kLlmsIndex
                   << "not found in" << kResourceRoot
                   << "- is the manual resource built into cavewherelib?";
        return;
    }

    //A page entry in llms.txt reads: - [Title](path/to/page.md): summary
    static const QRegularExpression itemRe(
        QStringLiteral("^-\\s+\\[([^\\]]+)\\]\\(([^)]+)\\):\\s*(.*)$"));

    QTextStream in(&file);
    while (!in.atEnd()) {
        const QString line = in.readLine();
        const QRegularExpressionMatch match = itemRe.match(line);
        if (!match.hasMatch()) {
            continue;
        }

        const QString relativePath = match.captured(2).trimmed();
        if (!relativePath.endsWith(kMarkdownSuffix)) {
            continue;
        }
        //Root-level entries (index.md is the landing, AUTHORING.md is a
        //contributor doc) are not registered as article pages.
        if (!relativePath.contains(QLatin1Char('/'))) {
            continue;
        }

        Article article;
        article.slug = slugForPath(relativePath);
        article.title = match.captured(1).trimmed();
        article.chapter = chapterForPath(relativePath);
        article.relativePath = relativePath;
        article.summary = match.captured(3).trimmed();

        m_slugToIndex.insert(article.slug, m_articles.size());
        m_articles.append(article);
    }
}

QVariantList cwManualIndex::articles() const
{
    QVariantList list;
    list.reserve(m_articles.size());
    for (const Article& article : m_articles) {
        QVariantMap map;
        map.insert(QStringLiteral("slug"), article.slug);
        map.insert(QStringLiteral("title"), article.title);
        map.insert(QStringLiteral("chapter"), article.chapter);
        map.insert(QStringLiteral("relativePath"), article.relativePath);
        map.insert(QStringLiteral("summary"), article.summary);
        list.append(map);
    }
    return list;
}

const QList<cwManualIndex::Article>& cwManualIndex::articleList() const
{
    return m_articles;
}

QString cwManualIndex::body(const QString& slug) const
{
    const int index = m_slugToIndex.value(slug, -1);
    if (index < 0) {
        qWarning() << "cwManualIndex: unknown manual slug" << slug;
        return QString();
    }
    return renderMarkdown(m_articles.at(index).relativePath);
}

QString cwManualIndex::landingBody() const
{
    return renderMarkdown(kLandingPage);
}

QString cwManualIndex::title(const QString& slug) const
{
    const int index = m_slugToIndex.value(slug, -1);
    return index < 0 ? QString() : m_articles.at(index).title;
}

QString cwManualIndex::slugForLink(const QString& fromSlug, const QString& href) const
{
    const QString trimmed = href.trimmed();
    if (trimmed.isEmpty() || trimmed.startsWith(QLatin1Char('#'))) {
        //Empty, or a same-page anchor the caller handles itself.
        return QString();
    }

    const QUrl target(trimmed);
    if (!target.scheme().isEmpty()) {
        //Absolute link (http, https, mailto, …) — not an in-app page.
        return QString();
    }

    //Resolve the (possibly relative) link against the current page's location
    //inside the manual, then map the resulting resource path back to a slug.
    const int fromIndex = m_slugToIndex.value(fromSlug, -1);
    const QString fromPath = fromIndex >= 0 ? m_articles.at(fromIndex).relativePath
                                            : kLandingPage;
    const QString resolved = QUrl(kResourceUrlRoot + fromPath).resolved(target).toString();
    if (!resolved.startsWith(kResourceUrlRoot)) {
        //The link resolved outside the manual tree.
        return QString();
    }

    QString relativePath = resolved.mid(kResourceUrlRoot.size());
    const int hash = relativePath.indexOf(QLatin1Char('#'));
    if (hash >= 0) {
        relativePath = relativePath.left(hash);
    }
    if (!relativePath.endsWith(kMarkdownSuffix)) {
        return QString();
    }

    const QString slug = slugForPath(relativePath);
    return m_slugToIndex.contains(slug) ? slug : QString();
}

QString cwManualIndex::searchText(const QString& slug) const
{
    const int index = m_slugToIndex.value(slug, -1);
    if (index < 0) {
        return QString();
    }

    const auto cached = m_searchTextCache.constFind(slug);
    if (cached != m_searchTextCache.constEnd()) {
        return cached.value();
    }

    const Article& article = m_articles.at(index);
    const QString haystack = (article.title + QLatin1Char(' ')
                              + article.summary + QLatin1Char(' ')
                              + frontMatterKeywords(article.relativePath)).toLower();
    m_searchTextCache.insert(slug, haystack);
    return haystack;
}

QString cwManualIndex::frontMatterKeywords(const QString& relativePath)
{
    QFile file(kResourceRoot + relativePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return QString();
    }

    //keywords live in the leading front-matter block as a YAML flow list:
    //  keywords: [scrap, carpet, morphing]
    //Return the comma-separated words space-joined; the brackets are dropped so
    //they don't pollute the search haystack.
    static const QRegularExpression keywordsRe(
        QStringLiteral("^keywords:\\s*\\[([^\\]]*)\\]"));

    QTextStream in(&file);
    bool insideFrontMatter = false;
    while (!in.atEnd()) {
        const QString line = in.readLine();
        if (line.trimmed() == QStringLiteral("---")) {
            if (!insideFrontMatter) {
                insideFrontMatter = true;
                continue;
            }
            break; //Closing delimiter — no keywords line in this article.
        }
        if (!insideFrontMatter) {
            continue;
        }
        const QRegularExpressionMatch match = keywordsRe.match(line);
        if (match.hasMatch()) {
            return match.captured(1).split(QLatin1Char(','), Qt::SkipEmptyParts)
                    .join(QLatin1Char(' '))
                    .simplified();
        }
    }
    return QString();
}

QString cwManualIndex::slugForPath(const QString& relativePath)
{
    QString base = relativePath;
    if (base.endsWith(kMarkdownSuffix)) {
        base.chop(kMarkdownSuffix.size());
    }
    base = base.toLower();

    static const QRegularExpression nonAlphaNumeric(QStringLiteral("[^a-z0-9]+"));
    base.replace(nonAlphaNumeric, QStringLiteral("-"));

    while (base.startsWith(QLatin1Char('-'))) {
        base.remove(0, 1);
    }
    while (base.endsWith(QLatin1Char('-'))) {
        base.chop(1);
    }
    return base;
}

QString cwManualIndex::chapterForPath(const QString& relativePath)
{
    //import-export/ holds two chapters; split it by filename so import and
    //export pages get their own headers (mirrors build-manual-html.py).
    static const QHash<QString, QString> chapterByFile = {
        {QStringLiteral("import-export/import-surveys.md"), QStringLiteral("Import")},
        {QStringLiteral("import-export/import-csv.md"), QStringLiteral("Import")},
        {QStringLiteral("import-export/export-surveys.md"), QStringLiteral("Export")},
        {QStringLiteral("import-export/export-a-map.md"), QStringLiteral("Export")},
    };
    const auto fileEntry = chapterByFile.constFind(relativePath);
    if (fileEntry != chapterByFile.constEnd()) {
        return fileEntry.value();
    }

    const int slash = relativePath.indexOf(QLatin1Char('/'));
    const QString directory = slash >= 0 ? relativePath.left(slash) : QString();

    static const QHash<QString, QString> chapterByDirectory = {
        {QStringLiteral("concepts"), QStringLiteral("Concepts")},
        {QStringLiteral("getting-started"), QStringLiteral("Getting Started")},
        {QStringLiteral("projects-and-files"), QStringLiteral("Projects & Files")},
        {QStringLiteral("view-3d"), QStringLiteral("3D View")},
        {QStringLiteral("survey-data"), QStringLiteral("Survey Data")},
        {QStringLiteral("notes"), QStringLiteral("Notes")},
        {QStringLiteral("scraps"), QStringLiteral("Scraps and Carpeting")},
        {QString(), QStringLiteral("Meta")},
    };
    const auto directoryEntry = chapterByDirectory.constFind(directory);
    if (directoryEntry != chapterByDirectory.constEnd()) {
        return directoryEntry.value();
    }

    const QString fallback = titleCase(directory);
    return fallback.isEmpty() ? QStringLiteral("Meta") : fallback;
}

QString cwManualIndex::renderMarkdown(const QString& relativePath) const
{
    QFile file(kResourceRoot + relativePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qWarning() << "cwManualIndex: cannot open manual page" << relativePath;
        return QString();
    }
    const QString text = QString::fromUtf8(file.readAll());
    return rewriteImageLinks(stripFrontMatter(text), relativePath);
}

QString cwManualIndex::stripFrontMatter(const QString& text)
{
    static const QRegularExpression frontMatter(
        QStringLiteral("\\A---\\r?\\n.*?\\r?\\n---\\r?\\n"),
        QRegularExpression::DotMatchesEverythingOption);

    const QRegularExpressionMatch match = frontMatter.match(text);
    if (match.hasMatch() && match.capturedStart() == 0) {
        return text.mid(match.capturedLength());
    }
    return text;
}

QString cwManualIndex::rewriteImageLinks(const QString& body, const QString& relativePath)
{
    const int slash = relativePath.lastIndexOf(QLatin1Char('/'));
    const QString directory = slash >= 0 ? relativePath.left(slash) : QString();
    QString baseUrl = kResourceUrlRoot;
    if (!directory.isEmpty()) {
        baseUrl += directory + QLatin1Char('/');
    }

    //Captures the "![alt](" prefix (group 1, kept verbatim) and the URL token up
    //to whitespace or the closing paren (group 2, the part we rewrite). An
    //optional Markdown title after the URL is left alone.
    static const QRegularExpression imageRe(
        QStringLiteral("(!\\[[^\\]]*\\]\\(\\s*)([^)\\s]+)"));

    QString out;
    out.reserve(body.size());
    int last = 0;
    QRegularExpressionMatchIterator it = imageRe.globalMatch(body);
    while (it.hasNext()) {
        const QRegularExpressionMatch match = it.next();
        out += body.mid(last, match.capturedStart(2) - last);
        out += resolveImageUrl(match.captured(2), baseUrl);
        last = match.capturedEnd(2);
    }
    out += body.mid(last);
    return out;
}
