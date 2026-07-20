#ifndef CWDOCUMENTFINDER_H
#define CWDOCUMENTFINDER_H

//Qt includes
#include <QObject>
#include <QPointer>
#include <QQmlEngine>
#include <QQuickTextDocument>
#include <QString>
#include <QList>

//Our includes
#include "CaveWhereLibExport.h"

class QTextDocument;

/**
 * @brief Text search and heading-anchor lookup over a rendered QTextDocument.
 *
 * The in-app manual renders each article into a read-only TextArea; this drives
 * that view's find-on-page bar and its same-page "#anchor" links. Both are pure
 * lookups into the document model. Set the TextArea's textDocument as @ref
 * document and a @ref query, and the finder collects every match so the bar can
 * step through them: @ref matchSelected fires for the current match (as you type
 * and on next/previous) with the character range for the view to select and
 * scroll into view. @ref headingPosition maps a GitHub-style anchor slug to the
 * character position of the matching heading, so a "#anchor" click can scroll to
 * it — the same slug scheme scripts/build-manual-html.py uses for the website.
 */
class CAVEWHERE_LIB_EXPORT cwDocumentFinder : public QObject
{
    Q_OBJECT
    QML_NAMED_ELEMENT(DocumentFinder)
    Q_PROPERTY(QQuickTextDocument* document READ document WRITE setDocument NOTIFY documentChanged)
    Q_PROPERTY(QString query READ query WRITE setQuery NOTIFY queryChanged)
    Q_PROPERTY(int matchCount READ matchCount NOTIFY matchesChanged)
    Q_PROPERTY(int currentMatch READ currentMatch NOTIFY currentMatchChanged)

public:
    explicit cwDocumentFinder(QObject* parent = nullptr);

    QQuickTextDocument* document() const { return m_document; }
    void setDocument(QQuickTextDocument* document);

    QString query() const { return m_query; }
    void setQuery(const QString& query);

    int matchCount() const { return static_cast<int>(m_matches.size()); }
    int currentMatch() const { return m_current + 1; } //!< 1-based, 0 when none

    Q_INVOKABLE void findNext();
    Q_INVOKABLE void findPrevious();

    //Character position of the heading whose GitHub-style slug matches @p anchor
    //(a leading '#' is ignored), or -1 if no heading matches.
    Q_INVOKABLE int headingPosition(const QString& anchor) const;

signals:
    void documentChanged();
    void queryChanged();
    void matchesChanged();
    void currentMatchChanged();

    //Emitted whenever a match becomes current (while typing, or on next/previous)
    //so the view can select the range and scroll it into view.
    void matchSelected(int position, int length);

private:
    QTextDocument* textDocument() const;
    void rebuildMatches();
    void selectCurrent();

    struct Match {
        int position = 0;
        int length = 0;
    };

    //Held by QPointer: the document is owned by the QML text view, which can be
    //destroyed while this finder outlives it; QPointer auto-nulls so textDocument()
    //stays safe.
    QPointer<QQuickTextDocument> m_document;
    QString m_query;
    QList<Match> m_matches;
    int m_current = -1; //!< 0-based index into m_matches, -1 when there is no match
};

#endif // CWDOCUMENTFINDER_H
