//Our includes
#include "cwTextDocumentObserver.h"

//Qt includes
#include <QQuickTextDocument>
#include <QTextDocument>

cwTextDocumentObserver::cwTextDocumentObserver(QObject* parent) :
    QObject(parent)
{
}

void cwTextDocumentObserver::setDocument(QQuickTextDocument* document)
{
    if (m_document == document) {
        return;
    }

    QObject::disconnect(m_contentsConnection);
    m_document = document;

    QTextDocument* doc = textDocument();
    if (doc != nullptr) {
        //A new article re-parses the same QTextDocument in place, so one connection
        //keeps us in sync across article changes.
        m_contentsConnection = connect(doc, &QTextDocument::contentsChanged,
                                       this, &cwTextDocumentObserver::onContentsChanged);
    }

    emit documentChanged();
    onDocumentReplaced();
}

QTextDocument* cwTextDocumentObserver::textDocument() const
{
    return m_document != nullptr ? m_document->textDocument() : nullptr;
}
