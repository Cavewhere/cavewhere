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
#include <QLineF>
#include <QPainterPath>
#include <QPointF>
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

struct ParsedLeaderSegment {
    QLineF segment;
};

struct ParsedSvg {
    QList<ParsedText>           texts;
    QList<ParsedImage>          images;
    QList<ParsedLeaderSegment>  leaderSegments;
};

// Hex color emitted by cwCaptureLeadLines for leader strokes (RGB 80,80,80).
constexpr const char* LeaderStrokeColor = "#505050";

// Shared SVG-numeric-list separator: any whitespace and/or commas. Used by
// both transform parsing and polyline `points` parsing.
static const QRegularExpression SvgNumSeparatorRe(QStringLiteral("[\\s,]+"));

// Parse an SVG `transform="matrix(a,b,c,d,e,f)"` (the form QSvgGenerator
// emits) into a QTransform. Returns identity for unrecognized strings.
QTransform parseTransformAttribute(const QString& attribute)
{
    static const QRegularExpression matrixRe(
        QStringLiteral("matrix\\s*\\(([^)]+)\\)"));

    const auto match = matrixRe.match(attribute);
    if(!match.hasMatch()) {
        return QTransform();
    }

    const QStringList parts = match.captured(1)
        .split(SvgNumSeparatorRe, Qt::SkipEmptyParts);
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
    // Tracks whether the current group nesting is inside a stroke="#505050"
    // group emitted by cwCaptureLeadLines. Inherited from parent on push.
    QStack<bool> leaderStack;
    leaderStack.push(false);

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

                bool inheritedLeader = leaderStack.top();
                if(attrs.hasAttribute(QStringLiteral("stroke"))) {
                    inheritedLeader = attrs.value(QStringLiteral("stroke"))
                                          .compare(QLatin1String(LeaderStrokeColor),
                                                   Qt::CaseInsensitive) == 0;
                }
                leaderStack.push(inheritedLeader);
            } else if(name == QLatin1String("polyline") && leaderStack.top()) {
                // Single-segment leader line. Parse "x1,y1 x2,y2".
                const QStringList toks = attrs.value(QStringLiteral("points"))
                    .toString().split(SvgNumSeparatorRe, Qt::SkipEmptyParts);
                if(toks.size() >= 4) {
                    const QPointF p1(toks[0].toDouble(), toks[1].toDouble());
                    const QPointF p2(toks[2].toDouble(), toks[3].toDouble());
                    const QLineF local(p1, p2);
                    const QLineF scene(txStack.top().map(local.p1()),
                                       txStack.top().map(local.p2()));
                    out.leaderSegments.append({scene});
                }
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
                leaderStack.pop();
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

QList<SvgLeaderLeaderCollision> SvgOverlapAnalyzer::leaderLeaderCollisions(const QUrl& svgUrl) const
{
    const ParsedSvg parsed = parseSvgFile(svgUrl.toLocalFile());

    QList<SvgLeaderLeaderCollision> result;
    const int n = parsed.leaderSegments.size();
    for(int i = 0; i < n; i++) {
        const QLineF& a = parsed.leaderSegments[i].segment;
        for(int j = i + 1; j < n; j++) {
            const QLineF& b = parsed.leaderSegments[j].segment;
            QPointF intersection;
            if(!cwCaptureLabelPlacer::segmentsCross(a, b, &intersection)) {
                continue;
            }
            SvgLeaderLeaderCollision c;
            c.leaderAStart = a.p1();
            c.leaderAEnd   = a.p2();
            c.leaderBStart = b.p1();
            c.leaderBEnd   = b.p2();
            c.intersection = intersection;
            result.append(c);
        }
    }
    return result;
}

QList<SvgTextLeaderCollision> SvgOverlapAnalyzer::textLeaderCollisions(const QUrl& svgUrl) const
{
    const ParsedSvg parsed = parseSvgFile(svgUrl.toLocalFile());

    QList<SvgTextLeaderCollision> result;
    if(parsed.leaderSegments.isEmpty() || parsed.texts.isEmpty()) {
        return result;
    }

    for(const ParsedText& t : parsed.texts) {
        if(t.rect.isEmpty()) {
            continue;
        }
        // A leader anchors at one corner of its own label; that boundary
        // contact is not an overlap. Shrink the rect so the intersect check
        // ignores that touch point.
        const QRectF shrunk = t.rect.adjusted(
            DegenerateRectEpsilon,  DegenerateRectEpsilon,
            -DegenerateRectEpsilon, -DegenerateRectEpsilon);
        if(shrunk.width() <= 0.0 || shrunk.height() <= 0.0) {
            continue;
        }
        for(const ParsedLeaderSegment& seg : parsed.leaderSegments) {
            if(cwCaptureLabelPlacer::segmentIntersectsRect(seg.segment, shrunk)) {
                SvgTextLeaderCollision c;
                c.text = t.text;
                c.textRect = t.rect;
                c.leaderStart = seg.segment.p1();
                c.leaderEnd = seg.segment.p2();
                result.append(c);
            }
        }
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
