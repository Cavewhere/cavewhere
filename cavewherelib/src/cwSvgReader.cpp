/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#include "cwSvgReader.h"
#include "cwPDFSettings.h"
#include "cwUnits.h"

#include <QBuffer>
#include <QImageReader>
#include <QRegularExpression>
#include <QXmlStreamReader>
#include <cmath>

namespace {

constexpr double kSvgCssDpi = 96.0;
constexpr qint64 kMaxImageBytes = 256ll * 1024 * 1024;
constexpr qint64 kBytesPerPixel = 4;

enum class SvgUnit {
    Px,
    In,
    Cm,
    Mm,
    Pt,
    Pc
};

struct SvgLength {
    double value = 0.0;
    SvgUnit unit = SvgUnit::Px;
    bool valid = false;
};

struct SvgSize {
    SvgLength width;
    SvgLength height;
    QSizeF viewBoxSize;
    bool hasWidth = false;
    bool hasHeight = false;
    bool hasViewBox = false;
};

bool parseSvgLength(const QString& text, SvgLength* out)
{
    if (out == nullptr) {
        return false;
    }

    const QString trimmed = text.trimmed();
    if (trimmed.isEmpty()) {
        return false;
    }

    static const QRegularExpression lengthPattern(
        QStringLiteral("^([+-]?(?:\\d+\\.?\\d*|\\d*\\.\\d+)(?:[eE][+-]?\\d+)?)\\s*([a-zA-Z%]*)$"));
    const QRegularExpressionMatch match = lengthPattern.match(trimmed);
    if (!match.hasMatch()) {
        return false;
    }

    bool ok = false;
    const double value = match.captured(1).toDouble(&ok);
    if (!ok) {
        return false;
    }

    const QString unitText = match.captured(2).toLower();
    SvgUnit unit = SvgUnit::Px;
    if (unitText.isEmpty() || unitText == QStringLiteral("px")) {
        unit = SvgUnit::Px;
    } else if (unitText == QStringLiteral("in")) {
        unit = SvgUnit::In;
    } else if (unitText == QStringLiteral("cm")) {
        unit = SvgUnit::Cm;
    } else if (unitText == QStringLiteral("mm")) {
        unit = SvgUnit::Mm;
    } else if (unitText == QStringLiteral("pt")) {
        unit = SvgUnit::Pt;
    } else if (unitText == QStringLiteral("pc")) {
        unit = SvgUnit::Pc;
    } else {
        return false;
    }

    out->value = value;
    out->unit = unit;
    out->valid = true;
    return true;
}

double svgLengthToInches(const SvgLength& length)
{
    switch (length.unit) {
    case SvgUnit::Px:
        return length.value / kSvgCssDpi;
    case SvgUnit::In:
        return length.value;
    case SvgUnit::Cm:
        return length.value / 2.54;
    case SvgUnit::Mm:
        return length.value / 25.4;
    case SvgUnit::Pt:
        return length.value / 72.0;
    case SvgUnit::Pc:
        return length.value / 6.0;
    }
    return length.value / kSvgCssDpi;
}

SvgSize parseSvgSize(const QByteArray& data)
{
    SvgSize result;
    QXmlStreamReader reader(data);

    while (!reader.atEnd()) {
        reader.readNext();
        if (!reader.isStartElement()) {
            continue;
        }
        if (reader.name() != QStringLiteral("svg")) {
            continue;
        }

        const auto attributes = reader.attributes();
        if (attributes.hasAttribute(QStringLiteral("width"))) {
            const QString widthText = attributes.value(QStringLiteral("width")).toString();
            if (parseSvgLength(widthText, &result.width)) {
                result.hasWidth = true;
            }
        }

        if (attributes.hasAttribute(QStringLiteral("height"))) {
            const QString heightText = attributes.value(QStringLiteral("height")).toString();
            if (parseSvgLength(heightText, &result.height)) {
                result.hasHeight = true;
            }
        }

        if (attributes.hasAttribute(QStringLiteral("viewBox"))) {
            const QString viewBoxText = attributes.value(QStringLiteral("viewBox")).toString();
            QStringList parts = viewBoxText.split(QRegularExpression(QStringLiteral("[,\\s]+")),
                                                  Qt::SkipEmptyParts);
            if (parts.size() == 4) {
                bool okX = false;
                bool okY = false;
                bool okW = false;
                bool okH = false;
                const double x = parts.at(0).toDouble(&okX);
                const double y = parts.at(1).toDouble(&okY);
                const double w = parts.at(2).toDouble(&okW);
                const double h = parts.at(3).toDouble(&okH);
                Q_UNUSED(x);
                Q_UNUSED(y);
                if (okW && okH && w > 0.0 && h > 0.0) {
                    result.viewBoxSize = QSizeF(w, h);
                    result.hasViewBox = true;
                }
            }
        }

        break;
    }

    return result;
}

QSize clampImageSize(const QSize& size)
{
    if (!size.isValid()) {
        return size;
    }

    const qint64 maxPixels = kMaxImageBytes / kBytesPerPixel;
    const qint64 pixelCount = static_cast<qint64>(size.width()) * size.height();
    if (pixelCount <= maxPixels) {
        return size;
    }

    const double scale = std::sqrt(static_cast<double>(maxPixels) / static_cast<double>(pixelCount));
    const int width = std::max(1, static_cast<int>(std::floor(size.width() * scale)));
    const int height = std::max(1, static_cast<int>(std::floor(size.height() * scale)));
    return QSize(width, height);
}

} // namespace

bool cwSvgReader::isSvg(const QByteArray& format)
{
    return QString::fromLatin1(format).compare(QStringLiteral("svg"), Qt::CaseInsensitive) == 0;
}

QImage cwSvgReader::toImage(QByteArray& data, const QByteArray& format)
{
    QBuffer stream(&data);
    QImageReader imageReader(&stream, format);
    imageReader.setAutoTransform(true);

    const int resolutionPpi = cwPDFSettings::instance()->resolutionImport();
    const QSize baseSize = imageReader.size();
    if (resolutionPpi > 0) {
        const SvgSize svgSize = parseSvgSize(data);
        const double scale = resolutionPpi / kSvgCssDpi;
        double widthPx = 0.0;
        double heightPx = 0.0;
        bool widthSet = false;
        bool heightSet = false;

        if (svgSize.hasWidth) {
            widthPx = svgLengthToInches(svgSize.width) * resolutionPpi;
            widthSet = widthPx > 0.0;
        }
        if (svgSize.hasHeight) {
            heightPx = svgLengthToInches(svgSize.height) * resolutionPpi;
            heightSet = heightPx > 0.0;
        }

        QSizeF aspectSource;
        if (svgSize.hasViewBox && svgSize.viewBoxSize.isValid()) {
            aspectSource = svgSize.viewBoxSize;
        } else if (baseSize.isValid()) {
            aspectSource = baseSize;
        }

        if (widthSet && !heightSet && aspectSource.isValid()) {
            heightPx = widthPx * (aspectSource.height() / aspectSource.width());
            heightSet = heightPx > 0.0;
        } else if (heightSet && !widthSet && aspectSource.isValid()) {
            widthPx = heightPx * (aspectSource.width() / aspectSource.height());
            widthSet = widthPx > 0.0;
        } else if (!widthSet && !heightSet && aspectSource.isValid()) {
            widthPx = aspectSource.width() * scale;
            heightPx = aspectSource.height() * scale;
            widthSet = widthPx > 0.0;
            heightSet = heightPx > 0.0;
        }

        if (widthSet && heightSet) {
            QSize scaledSize(
                std::max(1, qRound(widthPx)),
                std::max(1, qRound(heightPx)));
            const QSize clampedSize = clampImageSize(scaledSize);
            if (clampedSize != scaledSize) {
                qWarning() << "SVG render size clamped from" << scaledSize << "to" << clampedSize;
            }
            imageReader.setScaledSize(clampedSize);
        }
    }

    QImage image = imageReader.read();
    if (!image.isNull() && resolutionPpi > 0) {
        const int dotsPerMeter = qRound(cwUnits::convert(
            resolutionPpi,
            cwUnits::DotsPerInch,
            cwUnits::DotsPerMeter));
        image.setDotsPerMeterX(dotsPerMeter);
        image.setDotsPerMeterY(dotsPerMeter);
    }

    return image;
}
