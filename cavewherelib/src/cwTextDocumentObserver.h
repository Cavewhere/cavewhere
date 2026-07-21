#ifndef CWTEXTDOCUMENTOBSERVER_H
#define CWTEXTDOCUMENTOBSERVER_H

//Qt includes
#include <QObject>
#include <QPointer>
#include <QQuickTextDocument>

//Our includes
#include "CaveWhereLibExport.h"

class QTextDocument;

/**
 * @brief Base for objects that observe a QML text view's QTextDocument.
 *
 * cwDocumentFinder and cwDocumentStyler both attach to the QQuickTextDocument of a
 * QC.TextArea and re-derive a cached view of it — the finder its list of match
 * positions, the styler its per-block formatting. This base owns the shared
 * plumbing: the @ref document property (held by QPointer, so it auto-nulls when the
 * QML view is destroyed under it) and a single QTextDocument::contentsChanged
 * connection, routed to @ref onContentsChanged so a subclass can re-sync when a new
 * article is parsed into the same document in place.
 *
 * Format-only invariant: a subclass that EDITS the document (the styler) may only
 * change character/block formats, never insert or remove text. A subclass that
 * caches character positions (the finder) shares the same document, and format-only
 * edits leave those cached positions valid — which is why @ref onContentsChanged
 * defaults to ignoring the change: the finder must not rebuild (and reset its
 * current match) merely because the styler restyled the same text.
 */
class CAVEWHERE_LIB_EXPORT cwTextDocumentObserver : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QQuickTextDocument* document READ document WRITE setDocument NOTIFY documentChanged)

public:
    explicit cwTextDocumentObserver(QObject* parent = nullptr);

    QQuickTextDocument* document() const { return m_document; }
    void setDocument(QQuickTextDocument* document);

signals:
    void documentChanged();

protected:
    //The plain QTextDocument behind the QML view, or nullptr when unset/destroyed.
    QTextDocument* textDocument() const;

    //Called after the observed QQuickTextDocument is replaced (including with null),
    //so the subclass can re-derive its cached view of the new document.
    virtual void onDocumentReplaced() = 0;

    //Called when the current document's contents change in place — a new article
    //re-parsed into the same document, or a sibling observer's format-only edits.
    //Defaults to ignoring the change: an observer that caches character positions
    //keeps them valid under format-only edits, so rebuilding here would only reset
    //transient state (e.g. a current-match index). An observer that must re-derive
    //from freshly parsed content overrides this.
    virtual void onContentsChanged() {}

private:
    //Held by QPointer: the document is owned by the QML text view, which can be
    //destroyed while this observer outlives it; QPointer auto-nulls so textDocument()
    //stays safe.
    QPointer<QQuickTextDocument> m_document;
    QMetaObject::Connection m_contentsConnection;
};

#endif // CWTEXTDOCUMENTOBSERVER_H
