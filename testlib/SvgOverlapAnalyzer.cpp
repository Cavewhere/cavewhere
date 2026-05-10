/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

// Our includes
#include "SvgOverlapAnalyzer.h"
#include "cwCaptureLabelPlacer.h"

// Qt includes
#include <QByteArray>
#include <QDebug>
#include <QFile>
#include <QFont>
#include <QImage>
#include <QPainterPath>
#include <QRegularExpression>
#include <QStack>
#include <QString>
#include <QTransform>
#include <QXmlStreamReader>
#include <QtMath>

namespace {

// Use the placer's source-of-truth threshold so the post-export overlap
// check sees obstacle pixels exactly as the placer did.
constexpr int    AlphaOpaqueThreshold = cwCaptureLabelPlacer::DefaultAlphaThreshold;
constexpr double DegenerateRectEpsilon = 1e-3;

struct ParsedText {
    QString text;
    QRectF  rect;
};

struct ParsedImage {
    QRectF sceneRect;
    QImage image;
};

struct ParsedSvg {
    QList<ParsedText>  texts;
    QList<ParsedImage> images;
};

// Parse an SVG `transform="matrix(a,b,c,d,e,f)"` (the form QSvgGenerator
// emits) into a QTransform. Returns identity for unrecognized strings.
QTransform parseTransformAttribute(const QString& attribute)
{
    static const QRegularExpression matrixRe(
        QStringLiteral("matrix\\s*\\(([^)]+)\\)"));
    static const QRegularExpression separatorRe(QStringLiteral("[\\s,]+"));

    const auto match = matrixRe.match(attribute);
    if(!match.hasMatch()) {
        return QTransform();
    }

    const QStringList parts = match.captured(1)
        .split(separatorRe, Qt::SkipEmptyParts);
    if(parts.size() != 6) {
        return QTransform();
    }

    return QTransform(parts[0].toDouble(), parts[1].toDouble(),
                      parts[2].toDouble(), parts[3].toDouble(),
                      parts[4].toDouble(), parts[5].toDouble());
}

// Construct a QFont from SVG text attributes. SVG `font-size` attribute is
// in user units (paper pixels at the export resolution), so we use
// setPixelSize. Sub-pixel rounding (e.g. 33.333 -> 33) is acceptable; it
// shifts the analyzer's measured glyph rect by < 1% which is much smaller
// than the alpha-threshold fuzz.
QFont buildFontFromSvgAttrs(const QString& family,
                            double sizePixels,
                            const QString& weight,
                            const QString& style)
{
    QFont font;
    if(!family.isEmpty()) {
        font.setFamily(family);
    }
    const int px = qMax(1, qRound(sizePixels));
    font.setPixelSize(px);

    bool weightOk = false;
    const int weightInt = weight.toInt(&weightOk);
    if(weightOk) {
        font.setWeight(static_cast<QFont::Weight>(weightInt));
    } else if(weight.compare(QStringLiteral("bold"), Qt::CaseInsensitive) == 0) {
        font.setBold(true);
    }

    if(style.compare(QStringLiteral("italic"), Qt::CaseInsensitive) == 0
       || style.compare(QStringLiteral("oblique"), Qt::CaseInsensitive) == 0) {
        font.setItalic(true);
    }
    return font;
}

// Tight glyph-ink bounds. QPainterPath::addText positions the baseline at
// the supplied point; boundingRect() returns the actual ink extent.
QRectF textInkRect(const QString& text,
                   const QFont& font,
                   double baselineX,
                   double baselineY)
{
    if(text.isEmpty()) {
        return QRectF();
    }
    QPainterPath path;
    path.addText(QPointF(baselineX, baselineY), font, text);
    return path.boundingRect();
}

// Decode a "data:image/<fmt>;base64,<payload>" URI into a QImage. Returns a
// null QImage on failure.
QImage decodeDataUriImage(const QString& href)
{
    constexpr auto dataPrefix = "data:image/";
    if(!href.startsWith(QLatin1String(dataPrefix))) {
        return QImage();
    }

    const int comma = href.indexOf(QLatin1Char(','));
    if(comma < 0) {
        return QImage();
    }

    const QByteArray payload = href.mid(comma + 1).toLatin1();
    const QByteArray decoded = QByteArray::fromBase64(payload);
    QImage img;
    img.loadFromData(decoded);
    return img;
}

// Look up an attribute that may be either bare ("href") or namespace-prefixed
// ("xlink:href") in the SVG.
QString attrEither(const QXmlStreamAttributes& attrs,
                   const char* primary,
                   const char* alternate)
{
    if(attrs.hasAttribute(primary)) {
        return attrs.value(primary).toString();
    }
    if(attrs.hasAttribute(alternate)) {
        return attrs.value(alternate).toString();
    }
    return QString();
}

ParsedSvg parseSvgFile(const QString& path)
{
    ParsedSvg out;

    QFile file(path);
    if(!file.open(QIODevice::ReadOnly)) {
        qWarning() << "[SvgOverlapAnalyzer] cannot open" << path
                   << ":" << file.errorString();
        return out;
    }

    QXmlStreamReader xml;
    xml.setNamespaceProcessing(false);
    xml.addData(file.readAll());

    QStack<QTransform> txStack;
    txStack.push(QTransform());

    while(!xml.atEnd()) {
        xml.readNext();

        if(xml.isStartElement()) {
            const QStringView name = xml.name();
            const QXmlStreamAttributes attrs = xml.attributes();

            if(name == QLatin1String("g")) {
                QTransform childTx = txStack.top();
                if(attrs.hasAttribute(QStringLiteral("transform"))) {
                    childTx = parseTransformAttribute(
                                  attrs.value(QStringLiteral("transform")).toString())
                              * childTx;
                }
                txStack.push(childTx);
            } else if(name == QLatin1String("text")) {
                const double x = attrs.value(QStringLiteral("x")).toDouble();
                const double y = attrs.value(QStringLiteral("y")).toDouble();
                const QString family = attrs.value(QStringLiteral("font-family")).toString();
                const double size = attrs.value(QStringLiteral("font-size")).toDouble();
                const QString weight = attrs.value(QStringLiteral("font-weight")).toString();
                const QString style = attrs.value(QStringLiteral("font-style")).toString();
                const QString textValue = xml.readElementText();

                if(!textValue.isEmpty() && size > 0.0) {
                    const QFont font = buildFontFromSvgAttrs(family, size, weight, style);
                    const QRectF localInk = textInkRect(textValue, font, x, y);
                    const QRectF sceneInk = txStack.top().mapRect(localInk);
                    out.texts.append({textValue, sceneInk});
                }
                // readElementText consumed the end tag for this <text>; loop continues.
            } else if(name == QLatin1String("image")) {
                const double x = attrs.value(QStringLiteral("x")).toDouble();
                const double y = attrs.value(QStringLiteral("y")).toDouble();
                const double w = attrs.value(QStringLiteral("width")).toDouble();
                const double h = attrs.value(QStringLiteral("height")).toDouble();
                const QString href = attrEither(attrs, "href", "xlink:href");

                QImage img = decodeDataUriImage(href);
                if(!img.isNull() && w > 0.0 && h > 0.0) {
                    if(img.format() != QImage::Format_ARGB32
                       && img.format() != QImage::Format_ARGB32_Premultiplied) {
                        img = img.convertToFormat(QImage::Format_ARGB32);
                    }
                    const QRectF sceneRect = txStack.top().mapRect(QRectF(x, y, w, h));
                    out.images.append({sceneRect, img});
                }
            }
        } else if(xml.isEndElement()) {
            if(xml.name() == QLatin1String("g") && txStack.size() > 1) {
                txStack.pop();
            }
        }
    }

    if(xml.hasError()) {
        qWarning() << "[SvgOverlapAnalyzer] XML parse error:" << xml.errorString();
    }

    return out;
}

} // anonymous namespace

SvgOverlapAnalyzer::SvgOverlapAnalyzer(QObject* parent)
    : QObject(parent)
{
}

QList<SvgPassageOverlap> SvgOverlapAnalyzer::passageOverlaps(const QUrl& svgUrl) const
{
    const ParsedSvg parsed = parseSvgFile(svgUrl.toLocalFile());

    QList<SvgPassageOverlap> result;
    result.reserve(parsed.texts.size());

    for(const ParsedText& t : parsed.texts) {
        SvgPassageOverlap entry;
        entry.text = t.text;
        entry.rect = t.rect;

        if(t.rect.width() <= DegenerateRectEpsilon
           || t.rect.height() <= DegenerateRectEpsilon) {
            result.append(entry);
            continue;
        }

        int totalPixels = 0;
        int opaquePixels = 0;

        for(const ParsedImage& img : parsed.images) {
            const QRectF intersection = t.rect.intersected(img.sceneRect);
            if(intersection.isEmpty()) {
                continue;
            }

            const double xScale = double(img.image.width()) / img.sceneRect.width();
            const double yScale = double(img.image.height()) / img.sceneRect.height();

            const int px0 = qBound(0,
                qFloor((intersection.left() - img.sceneRect.left()) * xScale),
                img.image.width());
            const int py0 = qBound(0,
                qFloor((intersection.top() - img.sceneRect.top()) * yScale),
                img.image.height());
            const int px1 = qBound(0,
                qCeil((intersection.right() - img.sceneRect.left()) * xScale),
                img.image.width());
            const int py1 = qBound(0,
                qCeil((intersection.bottom() - img.sceneRect.top()) * yScale),
                img.image.height());

            for(int py = py0; py < py1; py++) {
                const QRgb* row = reinterpret_cast<const QRgb*>(img.image.constScanLine(py));
                for(int px = px0; px < px1; px++) {
                    ++totalPixels;
                    if(qAlpha(row[px]) > AlphaOpaqueThreshold) {
                        ++opaquePixels;
                    }
                }
            }
        }

        entry.overlapPixels = opaquePixels;
        entry.totalPixels = totalPixels;
        entry.overlapPercent = totalPixels > 0
            ? 100.0 * double(opaquePixels) / double(totalPixels)
            : 0.0;
        result.append(entry);
    }

    return result;
}

QList<SvgTextCollision> SvgOverlapAnalyzer::textCollisions(const QUrl& svgUrl) const
{
    const ParsedSvg parsed = parseSvgFile(svgUrl.toLocalFile());

    QList<SvgTextCollision> result;

    for(int i = 0; i < parsed.texts.size(); i++) {
        const ParsedText& a = parsed.texts[i];
        if(a.rect.isEmpty()) {
            continue;
        }
        for(int j = i + 1; j < parsed.texts.size(); j++) {
            const ParsedText& b = parsed.texts[j];
            if(b.rect.isEmpty()) {
                continue;
            }

            const QRectF intersection = a.rect.intersected(b.rect);
            if(intersection.width() <= DegenerateRectEpsilon
               || intersection.height() <= DegenerateRectEpsilon) {
                continue;
            }

            SvgTextCollision c;
            c.textA = a.text;
            c.textB = b.text;
            c.rectA = a.rect;
            c.rectB = b.rect;
            c.overlapArea = intersection.width() * intersection.height();

            const double areaA = a.rect.width() * a.rect.height();
            const double areaB = b.rect.width() * b.rect.height();
            const double minArea = qMin(areaA, areaB);
            c.overlapPercent = minArea > 0.0
                ? 100.0 * c.overlapArea / minArea
                : 0.0;

            result.append(c);
        }
    }

    return result;
}
