/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Our includes
#include "cwSymbologyPaletteIO.h"
#include "cwSymbologyPaletteData.h"
#include "cwSymbologyGlyph.h"
#include "cwSymbologyName.h"
#include "cwLineBrush.h"
#include "cwPenStroke.h"
#include "cavewhere_symbology.pb.h"

//Qt includes
#include <QColor>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>
#include <QUuid>

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

void glyphStrokeToProto(CavewhereSymbologyProto::GlyphStroke *proto, const cwPenStroke &stroke)
{
    proto->set_brushname(toStd(stroke.brushName));
    for (const auto &point : stroke.points) {
        auto *protoPoint = proto->add_points();
        protoPoint->mutable_position()->set_x(point.position.x());
        protoPoint->mutable_position()->set_y(point.position.y());
        protoPoint->set_pressure(point.pressure);
        protoPoint->set_timestampms(point.timestampMs);
    }
    if (!stroke.id.isNull()) {
        proto->set_id(toStd(stroke.id.toString(QUuid::WithoutBraces)));
    }
}

cwPenStroke glyphStrokeFromProto(const CavewhereSymbologyProto::GlyphStroke &proto)
{
    cwPenStroke stroke;
    stroke.brushName = fromStd(proto.brushname());
    stroke.points.reserve(proto.points_size());
    for (const auto &protoPoint : proto.points()) {
        cwPenPoint point;
        point.position = QPointF(protoPoint.position().x(), protoPoint.position().y());
        point.pressure = protoPoint.has_pressure() ? protoPoint.pressure() : -1.0;
        point.timestampMs = protoPoint.timestampms();
        stroke.points.append(point);
    }
    const QString id = fromStd(proto.id());
    if (!id.isEmpty()) {
        stroke.id = QUuid::fromString(id);
    }
    return stroke;
}

void glyphToProto(CavewhereSymbologyProto::Glyph *proto, const cwSymbologyGlyph &glyph)
{
    proto->set_name(toStd(glyph.name));
    proto->set_displayname(toStd(glyph.displayName));
    for (const auto &stroke : glyph.strokes) {
        glyphStrokeToProto(proto->add_strokes(), stroke);
    }
}

cwSymbologyGlyph glyphFromProto(const CavewhereSymbologyProto::Glyph &proto)
{
    cwSymbologyGlyph glyph;
    glyph.name = fromStd(proto.name());
    glyph.displayName = fromStd(proto.displayname());
    glyph.strokes.reserve(proto.strokes_size());
    for (const auto &protoStroke : proto.strokes()) {
        glyph.strokes.append(glyphStrokeFromProto(protoStroke));
    }
    return glyph;
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
    // Only glyph metadata lives in the palette file; the strokes round-trip
    // through per-glyph files in load()/save().
    for (const auto &glyph : palette.glyphs) {
        auto *meta = proto->add_glyphs();
        meta->set_name(toStd(glyph.name));
        meta->set_displayname(toStd(glyph.displayName));
    }
}

// Builds a palette from `proto`, enforcing the brush-name uniqueness invariant.
// Returns an error naming the duplicate on failure.
Monad::Result<cwSymbologyPaletteData> paletteFromProto(const CavewhereSymbologyProto::Palette &proto)
{
    cwSymbologyPaletteData palette;
    palette.lineBrushes.reserve(proto.linebrushes_size());
    for (const auto &protoBrush : proto.linebrushes()) {
        cwLineBrush brush = brushFromProto(protoBrush);
        // A brush name is the cross-palette substitution key and (after the
        // directory-as-truth refactor) a path component; enforce the shared
        // kebab-case rule so it stays a safe machine identifier.
        if (!cwSymbology::isValidName(brush.name)) {
            return Monad::Result<cwSymbologyPaletteData>(
                QStringLiteral("%1invalid brush name \"%2\"; brush names must be kebab-case "
                               "(lowercase letters, digits and hyphens)")
                    .arg(kLoadErrorPrefix, brush.name));
        }
        palette.lineBrushes.append(std::move(brush));
    }

    const QString duplicate = cwSymbologyPaletteData::findDuplicateBrushName(palette.lineBrushes);
    if (!duplicate.isEmpty()) {
        return Monad::Result<cwSymbologyPaletteData>(
            QStringLiteral("%1duplicate brush name \"%2\"")
                .arg(kLoadErrorPrefix, duplicate));
    }

    // Glyph metadata only — strokes are filled in by load() from per-glyph files.
    palette.glyphs.reserve(proto.glyphs_size());
    for (const auto &meta : proto.glyphs()) {
        cwSymbologyGlyph glyph;
        glyph.name = fromStd(meta.name());
        glyph.displayName = fromStd(meta.displayname());
        // The name becomes a path component (glyphs/<name>.cwglyph) in load();
        // reject anything that is not a plain kebab-case identifier so an
        // untrusted palette cannot read outside the palette directory.
        if (!cwSymbology::isValidName(glyph.name)) {
            return Monad::Result<cwSymbologyPaletteData>(
                QStringLiteral("%1invalid glyph name \"%2\"; glyph names must be kebab-case "
                               "(lowercase letters, digits and hyphens)")
                    .arg(kLoadErrorPrefix, glyph.name));
        }
        palette.glyphs.append(glyph);
    }

    const QString duplicateGlyph = cwSymbologyPaletteData::findDuplicateGlyphName(palette.glyphs);
    if (!duplicateGlyph.isEmpty()) {
        return Monad::Result<cwSymbologyPaletteData>(
            QStringLiteral("%1duplicate glyph name \"%2\"")
                .arg(kLoadErrorPrefix, duplicateGlyph));
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

Monad::Result<QByteArray> serializeJson(const google::protobuf::Message &proto)
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
const QString kGlyphsSubdirName = QStringLiteral("glyphs");
const QString kGlyphFileSuffix = QStringLiteral(".cwglyph");

QByteArray glyphToJson(const cwSymbologyGlyph &glyph)
{
    CavewhereSymbologyProto::Glyph proto;
    glyphToProto(&proto, glyph);
    const auto result = serializeJson(proto);
    if (result.hasError()) {
        qWarning("%s", qPrintable(result.errorMessage()));
        return QByteArray();
    }
    return result.value();
}

Monad::Result<cwSymbologyGlyph> glyphFromJson(const QByteArray &json)
{
    CavewhereSymbologyProto::Glyph proto;
    const auto status = google::protobuf::util::JsonStringToMessage(
        std::string(json.constData(), static_cast<size_t>(json.size())), &proto);
    if (!status.ok()) {
        return Monad::Result<cwSymbologyGlyph>(
            QStringLiteral("%1glyph JSON parse error: %2")
                .arg(kLoadErrorPrefix, QString::fromUtf8(status.ToString().c_str())));
    }
    return Monad::Result<cwSymbologyGlyph>(glyphFromProto(proto));
}

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

    for (const auto &glyph : palette.glyphs) {
        const auto glyphResult = saveGlyph(glyph, directory);
        if (glyphResult.hasError()) {
            return glyphResult;
        }
    }

    return Monad::ResultBase();
}

Monad::ResultBase saveGlyph(const cwSymbologyGlyph &glyph, const QString &directory)
{
    // The name becomes the file name (glyphs/<name>.cwglyph); reject anything
    // that is not a plain kebab-case identifier so it cannot escape glyphs/.
    if (!cwSymbology::isValidName(glyph.name)) {
        return Monad::ResultBase(
            QStringLiteral("%1invalid glyph name \"%2\"; glyph names must be kebab-case "
                           "(lowercase letters, digits and hyphens)")
                .arg(kSaveErrorPrefix, glyph.name));
    }

    const QString glyphsDir = QDir(directory).filePath(kGlyphsSubdirName);
    if (!QDir().mkpath(glyphsDir)) {
        return Monad::ResultBase(
            QStringLiteral("%1cannot create directory: %2").arg(kSaveErrorPrefix, glyphsDir));
    }

    CavewhereSymbologyProto::Glyph proto;
    glyphToProto(&proto, glyph);
    const auto json = serializeJson(proto);
    if (json.hasError()) {
        return Monad::ResultBase(json.errorMessage());
    }

    QSaveFile file(QDir(glyphsDir).filePath(glyph.name + kGlyphFileSuffix));
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

Monad::Result<cwSymbologyGlyph> loadGlyph(const QString &directory, const QString &glyphName)
{
    const QString path = QDir(QDir(directory).filePath(kGlyphsSubdirName))
                             .filePath(glyphName + kGlyphFileSuffix);
    QFile file(path);
    if (!file.open(QFile::ReadOnly)) {
        return Monad::Result<cwSymbologyGlyph>(
            QStringLiteral("%1cannot open glyph file %2: %3")
                .arg(kLoadErrorPrefix, path, file.errorString()));
    }
    const QByteArray contents = file.readAll();
    file.close();
    return glyphFromJson(contents);
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

    auto paletteResult = fromJson(contents);
    if (paletteResult.hasError()) {
        return paletteResult;
    }

    // fromJson populates glyph metadata only; pull each glyph's strokes from its
    // own file and assemble the full glyph (name from the metadata index).
    cwSymbologyPaletteData palette = paletteResult.value();
    for (auto &glyph : palette.glyphs) {
        auto glyphResult = loadGlyph(directory, glyph.name);
        if (glyphResult.hasError()) {
            return Monad::Result<cwSymbologyPaletteData>(glyphResult.errorMessage());
        }
        cwSymbologyGlyph loaded = glyphResult.value();
        if (loaded.name != glyph.name) {
            return Monad::Result<cwSymbologyPaletteData>(
                QStringLiteral("%1glyph file for \"%2\" declares mismatched name \"%3\"")
                    .arg(kLoadErrorPrefix, glyph.name, loaded.name));
        }
        glyph = std::move(loaded);
    }

    // Glyph strokes are now present, so the glyph → brush → glyph graph can be
    // checked for cycles (Decision 9). A cycle would recurse forever at
    // tessellation time, so reject the palette here.
    const QString cycle = palette.findGlyphDependencyCycle();
    if (!cycle.isEmpty()) {
        return Monad::Result<cwSymbologyPaletteData>(
            QStringLiteral("%1glyph dependency cycle: %2").arg(kLoadErrorPrefix, cycle));
    }

    return Monad::Result<cwSymbologyPaletteData>(palette);
}

} // namespace cwSymbologyPaletteIO
