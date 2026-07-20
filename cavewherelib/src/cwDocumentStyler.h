#ifndef CWDOCUMENTSTYLER_H
#define CWDOCUMENTSTYLER_H

//Qt includes
#include <QColor>
#include <QHash>
#include <QObject>
#include <QPointer>
#include <QQmlEngine>
#include <QQuickTextDocument>
#include <QSize>

//Our includes
#include "CaveWhereLibExport.h"

class QTextBlock;
class QTextDocument;

/**
 * @brief Applies readable article styling to a rendered Markdown QTextDocument.
 *
 * QC.TextArea renders the manual with textFormat: MarkdownText, and the Markdown
 * importer's block spacing, heading sizes, and image widths don't match the web
 * manual (and QTextDocument::setDefaultStyleSheet only reaches HTML, not
 * Markdown). This post-processes the parsed document instead: it sets heading
 * font sizes and per-block margins scaled from @ref baseFontSize, recolors links to
 * @ref linkColor (the imported anchor color doesn't track the theme), and — the part
 * no stylesheet can do — clamps each image to @ref contentWidth so figures shrink
 * with a narrowing window instead of overflowing the reading column.
 *
 * The pass re-runs whenever the document is re-parsed (a new article), the reading
 * width changes, or the base size changes, so it stays in sync with the view.
 */
class CAVEWHERE_LIB_EXPORT cwDocumentStyler : public QObject
{
    Q_OBJECT
    QML_NAMED_ELEMENT(DocumentStyler)
    Q_PROPERTY(QQuickTextDocument* document READ document WRITE setDocument NOTIFY documentChanged)
    Q_PROPERTY(int baseFontSize READ baseFontSize WRITE setBaseFontSize NOTIFY baseFontSizeChanged)
    Q_PROPERTY(qreal contentWidth READ contentWidth WRITE setContentWidth NOTIFY contentWidthChanged)
    Q_PROPERTY(QColor mutedColor READ mutedColor WRITE setMutedColor NOTIFY mutedColorChanged)
    Q_PROPERTY(QColor linkColor READ linkColor WRITE setLinkColor NOTIFY linkColorChanged)
    Q_PROPERTY(QString headingFontFamily READ headingFontFamily WRITE setHeadingFontFamily NOTIFY headingFontFamilyChanged)

public:
    explicit cwDocumentStyler(QObject* parent = nullptr);

    QQuickTextDocument* document() const { return m_document; }
    void setDocument(QQuickTextDocument* document);

    int baseFontSize() const { return m_baseFontSize; }
    void setBaseFontSize(int size);

    qreal contentWidth() const { return m_contentWidth; }
    void setContentWidth(qreal width);

    QColor mutedColor() const { return m_mutedColor; }
    void setMutedColor(const QColor& color);

    QColor linkColor() const { return m_linkColor; }
    void setLinkColor(const QColor& color);

    QString headingFontFamily() const { return m_headingFontFamily; }
    void setHeadingFontFamily(const QString& family);

signals:
    void documentChanged();
    void baseFontSizeChanged();
    void contentWidthChanged();
    void mutedColorChanged();
    void linkColorChanged();
    void headingFontFamilyChanged();

private:
    QTextDocument* textDocument() const;
    void apply();

    //Per-block styling steps; each takes the base font size so margins and heading
    //sizes track it.
    void styleBlock(const QTextBlock& block, bool isLeadParagraph, bool isImage);
    void styleImage(const QTextBlock& block);
    QSize naturalImageSize(const QString& url);

    //Held by QPointer: the document is owned by the QML text view, which can be
    //destroyed while this styler outlives it; QPointer auto-nulls so textDocument()
    //stays safe.
    QPointer<QQuickTextDocument> m_document;
    QMetaObject::Connection m_contentsConnection;

    int m_baseFontSize = 16;
    qreal m_contentWidth = 0.0;
    QColor m_mutedColor;
    QColor m_linkColor;
    QString m_headingFontFamily;

    QHash<QString, QSize> m_imageSizes; //!< url -> natural size, read once per image
    bool m_applying = false; //!< re-entrancy guard: our own edits re-emit contentsChanged
};

#endif // CWDOCUMENTSTYLER_H
