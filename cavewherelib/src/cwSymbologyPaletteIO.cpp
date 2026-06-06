/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Our includes
#include "cwSymbologyPaletteIO.h"
#include "cwSymbologyPaletteData.h"
#include "cwLineBrush.h"
#include "cavewhere_symbology.pb.h"

//Qt includes
#include <QColor>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>

//protobuf includes
#include <google/protobuf/util/json_util.h>

//Std includes
#include <string>

namespace {

const QString kLoadErrorPrefix = QStringLiteral("Loading symbology palette failed: ");
const QString kSaveErrorPrefix = QStringLiteral("Saving symbology palette failed: ");

// "#RRGGBBAA" hex layout: '#' followed by four 2-character components.
constexpr int kHexRgbaLength    = 9;
constexpr int kHexComponentWidth = 2;
constexpr int kHexRedOffset     = 1;
constexpr int kHexGreenOffset   = 3;
constexpr int kHexBlueOffset    = 5;
constexpr int kHexAlphaOffset   = 7;
constexpr int kHexBase          = 16;

std::string toStd(const QString &s)
{
    return s.toUtf8().toStdString();
}

QString fromStd(const std::string &s)
{
    return QString::fromUtf8(s.data(), static_cast<int>(s.size()));
}

// Colors round-trip as "#RRGGBB" (opaque) or "#RRGGBBAA" (with alpha).
// Empty string == invalid/unset color.
QString colorToHex(const QColor &color)
{
    if (!color.isValid()) {
        return QString();
    }
    if (color.alpha() == 255) {
        return color.name(QColor::HexRgb); // "#rrggbb"
    }
    return QStringLiteral("#%1%2%3%4")
        .arg(color.red(),   2, 16, QLatin1Char('0'))
        .arg(color.green(), 2, 16, QLatin1Char('0'))
        .arg(color.blue(),  2, 16, QLatin1Char('0'))
        .arg(color.alpha(), 2, 16, QLatin1Char('0'));
}

QColor colorFromHex(const QString &hex)
{
    if (hex.isEmpty()) {
        return QColor();
    }
    if (hex.size() == kHexRgbaLength && hex.startsWith(QLatin1Char('#'))) { // "#RRGGBBAA"
        bool rOk = false, gOk = false, bOk = false, aOk = false;
        const int r = hex.mid(kHexRedOffset,   kHexComponentWidth).toInt(&rOk, kHexBase);
        const int g = hex.mid(kHexGreenOffset, kHexComponentWidth).toInt(&gOk, kHexBase);
        const int b = hex.mid(kHexBlueOffset,  kHexComponentWidth).toInt(&bOk, kHexBase);
        const int a = hex.mid(kHexAlphaOffset, kHexComponentWidth).toInt(&aOk, kHexBase);
        if (rOk && gOk && bOk && aOk) {
            return QColor(r, g, b, a);
        }
        return QColor(); // malformed "#RRGGBBAA" — a later valid component must not mask an earlier bad one
    }
    return QColor::fromString(hex); // handles "#RRGGBB"
}

void layerToProto(CavewhereSymbologyProto::DecorationLayer *proto, const cwDecorationLayer &layer)
{
    proto->set_glyphname(toStd(layer.glyphName));
    proto->set_mode(static_cast<int>(layer.mode));
    for (const auto &rule : layer.rules) {
        auto *protoRule = proto->add_rules();
        protoRule->set_name(toStd(rule.name));
        protoRule->set_params(rule.params.constData(), static_cast<size_t>(rule.params.size()));
    }
    proto->set_buffermm(layer.bufferMm);
    proto->set_collisionpriority(layer.collisionPriority);
    proto->set_minpaperscale(layer.minPaperScale);
    proto->set_maxpaperscale(layer.maxPaperScale);

    proto->set_offsetcurvecolorlight(toStd(colorToHex(layer.offsetCurveColorLight)));
    proto->set_offsetcurvecolordark(toStd(colorToHex(layer.offsetCurveColorDark)));
    proto->set_offsetcurvewidthmm(layer.offsetCurveWidthMm);
    proto->set_offsetcurveoffsetmm(layer.offsetCurveOffsetMm);
    proto->set_offsetcurvedash(static_cast<int>(layer.offsetCurveDash));
    proto->set_offsetcurvecap(static_cast<int>(layer.offsetCurveCap));
    proto->set_offsetcurvejoin(static_cast<int>(layer.offsetCurveJoin));
    proto->set_offsetcurveacceptspressure(layer.offsetCurveAcceptsPressure);
}

cwDecorationLayer layerFromProto(const CavewhereSymbologyProto::DecorationLayer &proto)
{
    cwDecorationLayer layer;
    layer.glyphName = fromStd(proto.glyphname());
    layer.mode = static_cast<cwDecorationLayer::Mode>(proto.mode());
    layer.rules.reserve(proto.rules_size());
    for (const auto &protoRule : proto.rules()) {
        cwPlacementRuleData rule;
        rule.name = fromStd(protoRule.name());
        const std::string &params = protoRule.params();
        rule.params = QByteArray(params.data(), static_cast<int>(params.size()));
        layer.rules.append(rule);
    }
    // optional fields fall back to the value-type default when absent.
    if (proto.has_buffermm())          { layer.bufferMm = proto.buffermm(); }
    layer.collisionPriority = proto.collisionpriority();
    layer.minPaperScale = proto.minpaperscale();
    layer.maxPaperScale = proto.maxpaperscale();

    layer.offsetCurveColorLight = colorFromHex(fromStd(proto.offsetcurvecolorlight()));
    layer.offsetCurveColorDark  = colorFromHex(fromStd(proto.offsetcurvecolordark()));
    if (proto.has_offsetcurvewidthmm()) { layer.offsetCurveWidthMm = proto.offsetcurvewidthmm(); }
    layer.offsetCurveOffsetMm = proto.offsetcurveoffsetmm();
    if (proto.has_offsetcurvedash()) { layer.offsetCurveDash = static_cast<Qt::PenStyle>(proto.offsetcurvedash()); }
    if (proto.has_offsetcurvecap())  { layer.offsetCurveCap  = static_cast<Qt::PenCapStyle>(proto.offsetcurvecap()); }
    if (proto.has_offsetcurvejoin()) { layer.offsetCurveJoin = static_cast<Qt::PenJoinStyle>(proto.offsetcurvejoin()); }
    if (proto.has_offsetcurveacceptspressure()) {
        layer.offsetCurveAcceptsPressure = proto.offsetcurveacceptspressure();
    }
    return layer;
}

void brushToProto(CavewhereSymbologyProto::LineBrush *proto, const cwLineBrush &brush)
{
    proto->set_name(toStd(brush.name));
    proto->set_displayname(toStd(brush.displayName));
    proto->set_category(toStd(brush.category));
    proto->set_scrapoutline(brush.scrapOutline);
    proto->set_zorder(brush.zOrder);
    for (const auto &layer : brush.decorations) {
        layerToProto(proto->add_decorations(), layer);
    }
    proto->set_minpaperscale(brush.minPaperScale);
    proto->set_maxpaperscale(brush.maxPaperScale);
}

cwLineBrush brushFromProto(const CavewhereSymbologyProto::LineBrush &proto)
{
    cwLineBrush brush;
    brush.name = fromStd(proto.name());
    brush.displayName = fromStd(proto.displayname());
    brush.category = fromStd(proto.category());
    brush.scrapOutline = proto.scrapoutline();
    brush.zOrder = proto.zorder();
    brush.decorations.reserve(proto.decorations_size());
    for (const auto &protoLayer : proto.decorations()) {
        brush.decorations.append(layerFromProto(protoLayer));
    }
    brush.minPaperScale = proto.minpaperscale();
    brush.maxPaperScale = proto.maxpaperscale();
    return brush;
}

void paletteToProto(CavewhereSymbologyProto::Palette *proto, const cwSymbologyPaletteData &palette)
{
    proto->set_id(toStd(palette.id.toString(QUuid::WithoutBraces)));
    proto->set_name(toStd(palette.name));
    proto->set_description(toStd(palette.description));
    proto->set_author(toStd(palette.author));
    proto->set_version(toStd(palette.version));
    for (const auto &brush : palette.lineBrushes) {
        brushToProto(proto->add_linebrushes(), brush);
    }
}

// Builds a palette from `proto`, enforcing the brush-name uniqueness invariant.
// Returns an error naming the duplicate on failure.
Monad::Result<cwSymbologyPaletteData> paletteFromProto(const CavewhereSymbologyProto::Palette &proto)
{
    cwSymbologyPaletteData palette;
    palette.lineBrushes.reserve(proto.linebrushes_size());
    for (const auto &protoBrush : proto.linebrushes()) {
        palette.lineBrushes.append(brushFromProto(protoBrush));
    }

    const QString duplicate = cwSymbologyPaletteData::findDuplicateBrushName(palette.lineBrushes);
    if (!duplicate.isEmpty()) {
        return Monad::Result<cwSymbologyPaletteData>(
            QStringLiteral("%1duplicate brush name \"%2\"")
                .arg(kLoadErrorPrefix, duplicate));
    }

    palette.id = QUuid::fromString(fromStd(proto.id()));
    if (palette.id.isNull()) {
        return Monad::Result<cwSymbologyPaletteData>(
            QStringLiteral("%1palette has an invalid or empty id").arg(kLoadErrorPrefix));
    }
    palette.name = fromStd(proto.name());
    palette.description = fromStd(proto.description());
    palette.author = fromStd(proto.author());
    palette.version = fromStd(proto.version());
    return Monad::Result<cwSymbologyPaletteData>(palette);
}

Monad::Result<QByteArray> serializeJson(const CavewhereSymbologyProto::Palette &proto)
{
    std::string json;
    google::protobuf::util::JsonPrintOptions options;
    options.add_whitespace = true;
    options.always_print_fields_with_no_presence = true;
    const auto status = google::protobuf::util::MessageToJsonString(proto, &json, options);
    if (!status.ok()) {
        return Monad::Result<QByteArray>(
            QStringLiteral("%1JSON serialization failed: %2")
                .arg(kSaveErrorPrefix, QString::fromUtf8(status.ToString().c_str())));
    }
    return Monad::Result<QByteArray>(QByteArray(json.data(), static_cast<int>(json.size())));
}

} // namespace

namespace cwSymbologyPaletteIO {

const QString kPaletteJsonFileName = QStringLiteral("palette.json");

QByteArray toJson(const cwSymbologyPaletteData &palette)
{
    CavewhereSymbologyProto::Palette proto;
    paletteToProto(&proto, palette);
    const auto result = serializeJson(proto);
    if (result.hasError()) {
        qWarning("%s", qPrintable(result.errorMessage()));
        return QByteArray();
    }
    return result.value();
}

Monad::Result<cwSymbologyPaletteData> fromJson(const QByteArray &json)
{
    CavewhereSymbologyProto::Palette proto;
    const auto status = google::protobuf::util::JsonStringToMessage(
        std::string(json.constData(), static_cast<size_t>(json.size())), &proto);
    if (!status.ok()) {
        return Monad::Result<cwSymbologyPaletteData>(
            QStringLiteral("%1JSON parse error: %2")
                .arg(kLoadErrorPrefix, QString::fromUtf8(status.ToString().c_str())));
    }
    return paletteFromProto(proto);
}

Monad::ResultBase save(const cwSymbologyPaletteData &palette, const QString &directory)
{
    if (directory.isEmpty()) {
        return Monad::ResultBase(QStringLiteral("%1empty directory").arg(kSaveErrorPrefix));
    }
    if (!QDir().mkpath(directory)) {
        return Monad::ResultBase(
            QStringLiteral("%1cannot create directory: %2").arg(kSaveErrorPrefix, directory));
    }

    const QDir dir(directory);

    CavewhereSymbologyProto::Palette proto;
    paletteToProto(&proto, palette);

    const auto json = serializeJson(proto);
    if (json.hasError()) {
        return Monad::ResultBase(json.errorMessage());
    }

    QSaveFile file(dir.filePath(kPaletteJsonFileName));
    if (!file.open(QFile::WriteOnly)) {
        return Monad::ResultBase(
            QStringLiteral("%1cannot open %2 for write: %3")
                .arg(kSaveErrorPrefix, file.fileName(), file.errorString()));
    }
    file.write(json.value());
    if (!file.commit()) {
        return Monad::ResultBase(
            QStringLiteral("%1cannot write %2: %3")
                .arg(kSaveErrorPrefix, file.fileName(), file.errorString()));
    }
    return Monad::ResultBase();
}

Monad::Result<cwSymbologyPaletteData> load(const QString &directory)
{
    const QDir dir(directory);
    const QString jsonPath = dir.filePath(kPaletteJsonFileName);
    if (!QFileInfo::exists(jsonPath)) {
        return Monad::Result<cwSymbologyPaletteData>(
            QStringLiteral("%1no %2 in %3").arg(kLoadErrorPrefix, kPaletteJsonFileName, directory));
    }

    QFile file(jsonPath);
    if (!file.open(QFile::ReadOnly)) {
        return Monad::Result<cwSymbologyPaletteData>(
            QStringLiteral("%1cannot open %2: %3")
                .arg(kLoadErrorPrefix, jsonPath, file.errorString()));
    }
    const QByteArray contents = file.readAll();
    file.close();

    return fromJson(contents);
}

} // namespace cwSymbologyPaletteIO
