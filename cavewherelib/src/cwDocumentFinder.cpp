//Our includes
#include "cwDocumentFinder.h"

//Qt includes
#include <QQuickTextDocument>
#include <QTextDocument>
#include <QTextBlock>
#include <QTextCursor>
#include <QRegularExpression>

namespace {
    //GitHub-flavored-Markdown heading slug: lowercase, drop punctuation, spaces to
    //hyphens, collapse runs. This is what the manual's "#..." links resolve to, so
    //"## Keeping the map correct: loop closure" matches "#keeping-the-map-correct-loop-closure".
    //(The website build slugs headings with pandoc's GFM auto-identifiers instead;
    //the two agree for the plain-ASCII headings the manual uses.)
    QString headingSlug(const QString& text)
    {
        static const QRegularExpression nonSlug(QStringLiteral("[^a-z0-9 -]"));
        static const QRegularExpression whitespace(QStringLiteral("\\s+"));
        static const QRegularExpression dashRun(QStringLiteral("-+"));

        QString slug = text.toLower();
        slug.remove(nonSlug);
        slug.replace(whitespace, QStringLiteral("-"));
        slug.replace(dashRun, QStringLiteral("-"));
        while (slug.startsWith(QLatin1Char('-'))) {
            slug.remove(0, 1);
        }
        while (slug.endsWith(QLatin1Char('-'))) {
            slug.chop(1);
        }
        return slug;
    }
}

cwDocumentFinder::cwDocumentFinder(QObject* parent) :
    QObject(parent)
{
}

void cwDocumentFinder::setDocument(QQuickTextDocument* document)
{
    if (m_document == document) {
        return;
    }
    m_document = document;
    emit documentChanged();
    rebuildMatches();
}

void cwDocumentFinder::setQuery(const QString& query)
{
    if (m_query == query) {
        return;
    }
    m_query = query;
    emit queryChanged();
    rebuildMatches();
}

QTextDocument* cwDocumentFinder::textDocument() const
{
    return m_document != nullptr ? m_document->textDocument() : nullptr;
}

void cwDocumentFinder::rebuildMatches()
{
    const int previousCount = static_cast<int>(m_matches.size());
    const int previousCurrent = m_current;

    m_matches.clear();
    m_current = -1;

    QTextDocument* doc = textDocument();
    if (doc != nullptr && !m_query.isEmpty()) {
        //QTextDocument::find is a case-insensitive substring search over the
        //rendered text (formatting and image placeholders excluded).
        int from = 0;
        while (true) {
            const QTextCursor cursor = doc->find(m_query, from);
            if (cursor.isNull()) {
                break;
            }
            const int start = cursor.selectionStart();
            const int end = cursor.selectionEnd();
            m_matches.append({start, end - start});
            from = end > start ? end : end + 1; //guard against a zero-width match
        }
        if (!m_matches.isEmpty()) {
            m_current = 0;
        }
    }

    if (static_cast<int>(m_matches.size()) != previousCount) {
        emit matchesChanged();
    }
    if (m_current != previousCurrent) {
        emit currentMatchChanged();
    }
    selectCurrent();
}

void cwDocumentFinder::selectCurrent()
{
    if (m_current < 0 || m_current >= m_matches.size()) {
        return;
    }
    const Match& match = m_matches.at(m_current);
    emit matchSelected(match.position, match.length);
}

void cwDocumentFinder::findNext()
{
    if (m_matches.isEmpty()) {
        return;
    }
    m_current = (m_current + 1) % static_cast<int>(m_matches.size());
    emit currentMatchChanged();
    selectCurrent();
}

void cwDocumentFinder::findPrevious()
{
    if (m_matches.isEmpty()) {
        return;
    }
    const int count = static_cast<int>(m_matches.size());
    m_current = (m_current - 1 + count) % count;
    emit currentMatchChanged();
    selectCurrent();
}

int cwDocumentFinder::headingPosition(const QString& anchor) const
{
    QTextDocument* doc = textDocument();
    if (doc == nullptr) {
        return -1;
    }

    QString target = anchor;
    while (target.startsWith(QLatin1Char('#'))) {
        target.remove(0, 1);
    }
    target = target.toLower();
    if (target.isEmpty()) {
        return -1;
    }

    for (QTextBlock block = doc->begin(); block.isValid(); block = block.next()) {
        if (block.blockFormat().headingLevel() <= 0) {
            continue;
        }
        if (headingSlug(block.text()) == target) {
            return block.position();
        }
    }
    return -1;
}
