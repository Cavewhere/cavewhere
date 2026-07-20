//Our includes
#include "cwDocumentStyler.h"

//Qt includes
#include <QFont>
#include <QImageReader>
#include <QQuickTextDocument>
#include <QTextBlock>
#include <QTextCursor>
#include <QTextDocument>
#include <QTextFragment>
#include <QTextImageFormat>

//Std includes
#include <functional>

namespace {
    //Heading font sizes as a multiple of the base body size. These mirror the web
    //manual's scale (2.6 / 1.9 / 1.45 rem over a 1 rem body) so the two renderings
    //agree; levels past h3 taper gently toward the body size.
    constexpr double kHeadingSizeRatio[] = {1.0, 2.6, 1.9, 1.45, 1.15, 1.0, 0.9};
    constexpr double kLeadParagraphRatio = 1.08; //!< the muted intro line after an h1
    constexpr double kCaptionRatio = 0.86; //!< the italic caption sharing an image's block

    //Block margins as a multiple of the base body size. Qt's document layout adds
    //adjacent block margins rather than collapsing them (unlike CSS), so a heading's
    //space-above is its own top margin plus the previous block's bottom margin.
    constexpr double kParagraphBottom = 1.0;
    constexpr double kListItemBottom = 0.35;
    constexpr double kHeadingTop[] = {0.0, 0.3, 1.6, 1.3, 1.0, 0.9, 0.8};
    constexpr double kHeadingBottom[] = {0.0, 0.4, 0.4, 0.35, 0.3, 0.3, 0.3};
    constexpr double kImageTop = 1.4;
    constexpr double kImageBottom = 0.5;

    //Proportional line heights (percent of the font's natural line spacing, which
    //already carries ~1.2x leading — so this is NOT the CSS line-height multiple).
    //Generous but not airy for body copy; tight for the multi-line balance of heads.
    constexpr int kBodyLineHeight = 130;
    constexpr int kHeadingLineHeight = 104;

    constexpr int kMaxHeadingLevel = 6;

    bool blockHasImage(const QTextBlock& block)
    {
        for (QTextBlock::iterator it = block.begin(); !it.atEnd(); ++it) {
            if (it.fragment().isValid() && it.fragment().charFormat().isImageFormat()) {
                return true;
            }
        }
        return false;
    }

    //Replace a fragment's character format, preserving every property except those
    //@p apply overwrites (it starts from a copy of the current format).
    void mergeFragmentFormat(QTextDocument* doc, const QTextFragment& fragment,
                             const std::function<void(QTextCharFormat&)>& apply)
    {
        QTextCharFormat format = fragment.charFormat();
        apply(format);

        QTextCursor cursor(doc);
        cursor.setPosition(fragment.position());
        cursor.setPosition(fragment.position() + fragment.length(), QTextCursor::KeepAnchor);
        cursor.setCharFormat(format);
    }

    //QTextCharFormat has no pixel-size setter, so size (and optionally re-family)
    //through its font. The Markdown importer sizes headings with a relative
    //adjustment; clear it so our explicit size isn't scaled a second time.
    void setFormatFont(QTextCharFormat& format, int pixelSize, const QString& family)
    {
        QFont font = format.font();
        font.setPixelSize(pixelSize);
        if (!family.isEmpty()) {
            font.setFamily(family);
        }
        format.setFont(font);
        format.clearProperty(QTextFormat::FontSizeAdjustment);
    }
}

cwDocumentStyler::cwDocumentStyler(QObject* parent) :
    QObject(parent)
{
}

void cwDocumentStyler::setDocument(QQuickTextDocument* document)
{
    if (m_document == document) {
        return;
    }

    QObject::disconnect(m_contentsConnection);
    m_document = document;
    if (m_document != nullptr && m_document->textDocument() != nullptr) {
        //A new article re-parses the same QTextDocument in place, so one connection
        //keeps us in sync across article changes.
        m_contentsConnection = connect(m_document->textDocument(), &QTextDocument::contentsChanged,
                                       this, &cwDocumentStyler::apply);
    }

    emit documentChanged();
    apply();
}

void cwDocumentStyler::setBaseFontSize(int size)
{
    if (m_baseFontSize == size) {
        return;
    }
    m_baseFontSize = size;
    emit baseFontSizeChanged();
    apply();
}

void cwDocumentStyler::setContentWidth(qreal width)
{
    if (qFuzzyCompare(m_contentWidth, width)) {
        return;
    }
    m_contentWidth = width;
    emit contentWidthChanged();
    apply();
}

void cwDocumentStyler::setMutedColor(const QColor& color)
{
    if (m_mutedColor == color) {
        return;
    }
    m_mutedColor = color;
    emit mutedColorChanged();
    apply();
}

void cwDocumentStyler::setLinkColor(const QColor& color)
{
    if (m_linkColor == color) {
        return;
    }
    m_linkColor = color;
    emit linkColorChanged();
    apply();
}

void cwDocumentStyler::setHeadingFontFamily(const QString& family)
{
    if (m_headingFontFamily == family) {
        return;
    }
    m_headingFontFamily = family;
    emit headingFontFamilyChanged();
    apply();
}

QTextDocument* cwDocumentStyler::textDocument() const
{
    return m_document != nullptr ? m_document->textDocument() : nullptr;
}

void cwDocumentStyler::apply()
{
    //This pass must only ever change character/block FORMATS, never insert or
    //remove text: cwDocumentFinder shares this same document and caches matches by
    //character position, and format-only edits leave those positions valid.
    QTextDocument* doc = textDocument();
    if (doc == nullptr || m_applying) {
        return;
    }

    m_applying = true;

    QTextCursor guard(doc);
    guard.beginEditBlock();

    bool afterHeading1 = false;
    for (QTextBlock block = doc->begin(); block.isValid(); block = block.next()) {
        const int level = block.blockFormat().headingLevel();
        const bool isImage = blockHasImage(block);
        const bool isLead = afterHeading1 && level <= 0 && !isImage
                            && !block.text().trimmed().isEmpty();

        styleBlock(block, isLead, isImage);
        if (isImage) {
            styleImage(block);
        }

        if (level == 1) {
            afterHeading1 = true;
        } else if (level > 0 || !block.text().trimmed().isEmpty()) {
            //Any heading or the first real paragraph ends the run just under an h1.
            afterHeading1 = false;
        }
    }

    guard.endEditBlock();

    m_applying = false;
}

void cwDocumentStyler::styleBlock(const QTextBlock& block, bool isLeadParagraph, bool isImage)
{
    QTextDocument* doc = textDocument();
    const int level = block.blockFormat().headingLevel();
    const bool isHeading = level > 0 && level <= kMaxHeadingLevel;
    const bool isListItem = block.textList() != nullptr;

    //Block margins (in pixels), scaled from the base size.
    QTextBlockFormat blockFormat = block.blockFormat();
    if (isHeading) {
        blockFormat.setTopMargin(kHeadingTop[level] * m_baseFontSize);
        blockFormat.setBottomMargin(kHeadingBottom[level] * m_baseFontSize);
        blockFormat.setLineHeight(kHeadingLineHeight, QTextBlockFormat::ProportionalHeight);
    } else if (isImage) {
        blockFormat.setTopMargin(kImageTop * m_baseFontSize);
        blockFormat.setBottomMargin(kImageBottom * m_baseFontSize);
        blockFormat.setAlignment(Qt::AlignHCenter);
        blockFormat.setLineHeight(kHeadingLineHeight, QTextBlockFormat::ProportionalHeight);
    } else {
        blockFormat.setTopMargin(0.0);
        blockFormat.setBottomMargin((isListItem ? kListItemBottom : kParagraphBottom) * m_baseFontSize);
        blockFormat.setLineHeight(kBodyLineHeight, QTextBlockFormat::ProportionalHeight);
    }

    QTextCursor cursor(block);
    cursor.setBlockFormat(blockFormat);

    //A per-fragment pass is only needed to resize headings/lead text or to recolor
    //links. Plain body paragraphs otherwise inherit the view's default font, and
    //image blocks (captions included) are handled by styleImage.
    const bool sizeFragments = isHeading || isLeadParagraph;
    const bool recolorLinks = m_linkColor.isValid();
    if (isImage || (!sizeFragments && !recolorLinks)) {
        return;
    }

    const double ratio = isHeading ? kHeadingSizeRatio[level] : kLeadParagraphRatio;
    const int pixelSize = qRound(ratio * m_baseFontSize);
    const bool muteColor = isLeadParagraph && m_mutedColor.isValid();
    //Headings keep the brand display font; the lead paragraph stays in the body face.
    const QString family = isHeading ? m_headingFontFamily : QString();

    for (QTextBlock::iterator it = block.begin(); !it.atEnd(); ++it) {
        const QTextFragment fragment = it.fragment();
        if (!fragment.isValid() || fragment.charFormat().isImageFormat()) {
            continue;
        }
        mergeFragmentFormat(doc, fragment, [&](QTextCharFormat& format) {
            if (sizeFragments) {
                setFormatFont(format, pixelSize, family);
            }
            if (muteColor) {
                format.setForeground(m_mutedColor);
            }
            //A link overrides the body/muted color so it still reads as a link.
            if (recolorLinks && format.isAnchor()) {
                format.setForeground(m_linkColor);
            }
        });
    }
}

void cwDocumentStyler::styleImage(const QTextBlock& block)
{
    QTextDocument* doc = textDocument();
    const int captionSize = qRound(kCaptionRatio * m_baseFontSize);

    for (QTextBlock::iterator it = block.begin(); !it.atEnd(); ++it) {
        const QTextFragment fragment = it.fragment();
        if (!fragment.isValid()) {
            continue;
        }

        if (fragment.charFormat().isImageFormat()) {
            const QTextImageFormat imageFormat = fragment.charFormat().toImageFormat();
            const QSize natural = naturalImageSize(imageFormat.name());
            if (natural.isEmpty() || m_contentWidth <= 0.0) {
                continue;
            }

            //Only ever shrink: an image narrower than the column keeps its size.
            const double width = qMin<double>(natural.width(), m_contentWidth);
            const double height = width * natural.height() / natural.width();
            mergeFragmentFormat(doc, fragment, [&](QTextCharFormat& format) {
                QTextImageFormat scaled = format.toImageFormat();
                scaled.setWidth(width);
                scaled.setHeight(height);
                format = scaled;
            });
        } else if (m_mutedColor.isValid()) {
            //The italic caption sharing the image's block: smaller and muted.
            mergeFragmentFormat(doc, fragment, [&](QTextCharFormat& format) {
                setFormatFont(format, captionSize, QString());
                format.setForeground(m_mutedColor);
            });
        }
    }
}

QSize cwDocumentStyler::naturalImageSize(const QString& url)
{
    const QHash<QString, QSize>::const_iterator cached = m_imageSizes.constFind(url);
    if (cached != m_imageSizes.constEnd()) {
        return cached.value();
    }

    //Manual images resolve to "qrc:/manual/..."; QImageReader wants the ":/..."
    //resource path — drop the "qrc" scheme, keeping the ":/". Reading the header
    //alone gives the natural size cheaply.
    static const QString qrcScheme = QStringLiteral("qrc");
    QString path = url;
    if (path.startsWith(qrcScheme + QStringLiteral(":/"))) {
        path.remove(0, qrcScheme.size());
    }

    const QSize size = QImageReader(path).size();
    m_imageSizes.insert(url, size);
    return size;
}
