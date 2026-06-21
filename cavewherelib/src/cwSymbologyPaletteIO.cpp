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
#include "cwPlacementRuleParamsCodec.h"
#include "cwDecorationLayerValidator.h"
#include "cwPlacementRuleRegistry.h"
#include "cwSymbologyErrorCodes.h"
#include "cwError.h"
#include "cwRegionIOTask.h"
#include "cavewhereVersion.h"
#include "cavewhere_symbology.pb.h"
#include "cavewhere_common.pb.h"

//Qt includes
#include <QColor>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>
#include <QSet>
#include <QStringList>
#include <QUuid>

//protobuf includes
#include <google/protobuf/util/json_util.h>

//Std includes
#include <algorithm>
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

// Stamp the save/load format version onto a per-file message. Synced with the
// project clock: the same monotonic cwRegionIOTask::protoVersion() that
// cwSaveLoad writes, so palettes and projects gate on one version number.
void stampFileVersion(CavewhereCommonProto::FileVersion *proto)
{
    proto->set_version(cwRegionIOTask::protoVersion());
    proto->set_cavewhereversion(toStd(CavewhereVersion));
}

// The file's stamped format version, or 0 when unstamped (a pre-versioning file,
// which is always in range).
template <typename Proto>
int fileVersionOf(const Proto &proto)
{
    return proto.has_fileversion() ? proto.fileversion().version() : 0;
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
    for (const auto &rule : layer.rules) {
        auto *protoRule = proto->add_rules();
        protoRule->set_name(toStd(rule.name));
        const QByteArray bytes = cwPlacementRuleParamsCodec::encode(rule.name, rule.parameters);
        protoRule->set_params(bytes.constData(), static_cast<size_t>(bytes.size()));
        // Persist only the non-default (disabled) state — an unset field reads
        // back as enabled, matching the value-type default.
        if (!rule.enabled) {
            protoRule->set_enabled(false);
        }
    }
    proto->set_buffermm(layer.bufferMm);
    proto->set_collisionpriority(layer.collisionPriority);
    proto->set_minpaperscale(layer.minPaperScale);
    proto->set_maxpaperscale(layer.maxPaperScale);

    proto->set_linecolorlight(toStd(colorToHex(layer.lineColorLight)));
    proto->set_linecolordark(toStd(colorToHex(layer.lineColorDark)));
    proto->set_linewidthmm(layer.lineWidthMm);
    proto->set_linedash(static_cast<int>(layer.lineDash));
    proto->set_linecap(static_cast<int>(layer.lineCap));
    proto->set_linejoin(static_cast<int>(layer.lineJoin));
    proto->set_lineacceptspressure(layer.lineAcceptsPressure);

    proto->set_fillcolorlight(toStd(colorToHex(layer.fillColorLight)));
    proto->set_fillcolordark(toStd(colorToHex(layer.fillColorDark)));
}

cwDecorationLayer layerFromProto(const CavewhereSymbologyProto::DecorationLayer &proto)
{
    cwDecorationLayer layer;
    layer.glyphName = fromStd(proto.glyphname());
    layer.rules.reserve(proto.rules_size());
    for (const auto &protoRule : proto.rules()) {
        cwPlacementRuleData rule;
        rule.name = fromStd(protoRule.name());
        const std::string &params = protoRule.params();
        rule.parameters = cwPlacementRuleParamsCodec::decode(
            rule.name, QByteArray(params.data(), static_cast<int>(params.size())));
        if (protoRule.has_enabled()) {
            rule.enabled = protoRule.enabled();
        }
        layer.rules.append(rule);
    }
    // optional fields fall back to the value-type default when absent.
    if (proto.has_buffermm())          { layer.bufferMm = proto.buffermm(); }
    layer.collisionPriority = proto.collisionpriority();
    layer.minPaperScale = proto.minpaperscale();
    layer.maxPaperScale = proto.maxpaperscale();

    layer.lineColorLight = colorFromHex(fromStd(proto.linecolorlight()));
    layer.lineColorDark  = colorFromHex(fromStd(proto.linecolordark()));
    if (proto.has_linewidthmm()) { layer.lineWidthMm = proto.linewidthmm(); }
    if (proto.has_linedash()) { layer.lineDash = static_cast<Qt::PenStyle>(proto.linedash()); }
    if (proto.has_linecap())  { layer.lineCap  = static_cast<Qt::PenCapStyle>(proto.linecap()); }
    if (proto.has_linejoin()) { layer.lineJoin = static_cast<Qt::PenJoinStyle>(proto.linejoin()); }
    if (proto.has_lineacceptspressure()) {
        layer.lineAcceptsPressure = proto.lineacceptspressure();
    }

    layer.fillColorLight = colorFromHex(fromStd(proto.fillcolorlight()));
    layer.fillColorDark  = colorFromHex(fromStd(proto.fillcolordark()));
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
    stampFileVersion(proto->mutable_fileversion());
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

void glyphStrokeToProto(CavewhereCommonProto::Stroke *proto, const cwPenStroke &stroke)
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

cwPenStroke glyphStrokeFromProto(const CavewhereCommonProto::Stroke &proto)
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
    stampFileVersion(proto->mutable_fileversion());
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

// palette.json carries palette-level identity only; brushes and glyphs are
// per-file (the directory is their source of truth), so they never travel here.
void paletteToProto(CavewhereSymbologyProto::Palette *proto, const cwSymbologyPaletteData &palette)
{
    proto->set_id(toStd(palette.id.toString(QUuid::WithoutBraces)));
    proto->set_name(toStd(palette.name));
    proto->set_description(toStd(palette.description));
    proto->set_author(toStd(palette.author));
    proto->set_version(toStd(palette.version));
    stampFileVersion(proto->mutable_fileversion());
}

// Builds palette identity from `proto`. The directory layer fills lineBrushes
// and glyphs from the per-file payloads, so they are empty here.
Monad::Result<cwSymbologyPaletteData> paletteFromProto(const CavewhereSymbologyProto::Palette &proto)
{
    cwSymbologyPaletteData palette;
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

// Parse JSON bytes into `proto`, returning a load error on failure.
template <typename Proto>
Monad::ResultBase parseJsonInto(const QByteArray &json, Proto &proto, const QString &kind)
{
    const auto status = google::protobuf::util::JsonStringToMessage(
        std::string(json.constData(), static_cast<size_t>(json.size())), &proto);
    if (!status.ok()) {
        return Monad::ResultBase(
            QStringLiteral("%1%2 JSON parse error: %3")
                .arg(kLoadErrorPrefix, kind, QString::fromUtf8(status.ToString().c_str())));
    }
    return Monad::ResultBase();
}

// JSON -> value plus the file's stamped format version (the public *FromJson
// entry points discard the version; load() aggregates it for the gate).
Monad::Result<cwLineBrush> parseBrushJson(const QByteArray &json, int &fileVersion)
{
    CavewhereSymbologyProto::LineBrush proto;
    const auto parsed = parseJsonInto(json, proto, QStringLiteral("brush"));
    if (parsed.hasError()) {
        return Monad::Result<cwLineBrush>(parsed.errorMessage());
    }
    fileVersion = fileVersionOf(proto);
    return Monad::Result<cwLineBrush>(brushFromProto(proto));
}

Monad::Result<cwSymbologyGlyph> parseGlyphJson(const QByteArray &json, int &fileVersion)
{
    CavewhereSymbologyProto::Glyph proto;
    const auto parsed = parseJsonInto(json, proto, QStringLiteral("glyph"));
    if (parsed.hasError()) {
        return Monad::Result<cwSymbologyGlyph>(parsed.errorMessage());
    }
    fileVersion = fileVersionOf(proto);
    return Monad::Result<cwSymbologyGlyph>(glyphFromProto(proto));
}

Monad::Result<cwSymbologyPaletteData> parsePaletteJson(const QByteArray &json, int &fileVersion)
{
    CavewhereSymbologyProto::Palette proto;
    const auto parsed = parseJsonInto(json, proto, QStringLiteral("palette"));
    if (parsed.hasError()) {
        return Monad::Result<cwSymbologyPaletteData>(parsed.errorMessage());
    }
    fileVersion = fileVersionOf(proto);
    return paletteFromProto(proto);
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

// Serialize a proto message to JSON bytes, warning and returning empty on the
// (serialization-config) failure path shared by every *ToJson entry point.
template <typename Proto>
QByteArray serializeProtoToJson(const Proto &proto)
{
    const auto result = serializeJson(proto);
    if (result.hasError()) {
        qWarning("%s", qPrintable(result.errorMessage()));
        return QByteArray();
    }
    return result.value();
}

QString invalidNameMessage(const QString &prefix, const QString &entityKind, const QString &name)
{
    return QStringLiteral("%1invalid %2 name \"%3\"; %2 names must be kebab-case "
                          "(lowercase letters, digits and hyphens)")
        .arg(prefix, entityKind, name);
}

// Validate the name (kebab-case == path-safe), ensure <directory>/<subdir>
// exists, and atomically write <subdir>/<name><suffix>. Validation runs before
// anything is opened, so a rejected name writes nothing.
Monad::ResultBase writeNamedFile(const QString &name, const QString &entityKind,
                                 const QString &directory, const QString &subdirName,
                                 const QString &suffix, const QByteArray &contents)
{
    if (!cwSymbology::isValidName(name)) {
        return Monad::ResultBase(invalidNameMessage(kSaveErrorPrefix, entityKind, name));
    }
    const QString subdir = QDir(directory).filePath(subdirName);
    if (!QDir().mkpath(subdir)) {
        return Monad::ResultBase(
            QStringLiteral("%1cannot create directory: %2").arg(kSaveErrorPrefix, subdir));
    }
    QSaveFile file(QDir(subdir).filePath(name + suffix));
    if (!file.open(QFile::WriteOnly)) {
        return Monad::ResultBase(
            QStringLiteral("%1cannot open %2 for write: %3")
                .arg(kSaveErrorPrefix, file.fileName(), file.errorString()));
    }
    file.write(contents);
    if (!file.commit()) {
        return Monad::ResultBase(
            QStringLiteral("%1cannot write %2: %3")
                .arg(kSaveErrorPrefix, file.fileName(), file.errorString()));
    }
    return Monad::ResultBase();
}

Monad::Result<QByteArray> readNamedFile(const QString &directory, const QString &subdirName,
                                        const QString &suffix, const QString &name,
                                        const QString &entityKind)
{
    const QString path = QDir(QDir(directory).filePath(subdirName)).filePath(name + suffix);
    QFile file(path);
    if (!file.open(QFile::ReadOnly)) {
        return Monad::Result<QByteArray>(
            QStringLiteral("%1cannot open %2 file %3: %4")
                .arg(kLoadErrorPrefix, entityKind, path, file.errorString()));
    }
    return Monad::Result<QByteArray>(file.readAll());
}

Monad::ResultBase removeNamedFile(const QString &directory, const QString &subdirName,
                                  const QString &suffix, const QString &name,
                                  const QString &entityKind)
{
    if (!cwSymbology::isValidName(name)) {
        return Monad::ResultBase(invalidNameMessage(kSaveErrorPrefix, entityKind, name));
    }
    const QString path = QDir(QDir(directory).filePath(subdirName)).filePath(name + suffix);
    if (QFile::exists(path) && !QFile::remove(path)) {
        return Monad::ResultBase(
            QStringLiteral("%1cannot remove %2 file %3").arg(kSaveErrorPrefix, entityKind, path));
    }
    return Monad::ResultBase();
}

// Scans <directory>/<subdir> for *<suffix> files, sorted by name (deterministic
// load order). Each filename stem is the authoritative entity name: it must be
// kebab-case and must equal the payload's declared name. The directory is the
// source of truth, so the files present *are* the entity set.
template <typename T, typename LoadOne>
Monad::Result<QVector<T>> scanEntities(const QString &directory, const QString &subdirName,
                                       const QString &suffix, const QString &entityKind,
                                       LoadOne loadOne)
{
    QVector<T> entities;
    const QDir subdir(QDir(directory).filePath(subdirName));
    if (!subdir.exists()) {
        return Monad::Result<QVector<T>>(entities);
    }

    const QStringList files =
        subdir.entryList({QStringLiteral("*") + suffix}, QDir::Files, QDir::Name);
    entities.reserve(files.size());
    for (const QString &fileName : files) {
        const QString stem = fileName.chopped(suffix.size());
        if (!cwSymbology::isValidName(stem)) {
            return Monad::Result<QVector<T>>(invalidNameMessage(kLoadErrorPrefix, entityKind, stem));
        }
        auto loaded = loadOne(directory, stem);
        if (loaded.hasError()) {
            return Monad::Result<QVector<T>>(loaded.errorMessage());
        }
        T entity = loaded.value();
        if (entity.name != stem) {
            return Monad::Result<QVector<T>>(
                QStringLiteral("%1%2 file for \"%3\" declares mismatched name \"%4\"")
                    .arg(kLoadErrorPrefix, entityKind, stem, entity.name));
        }
        entities.append(std::move(entity));
    }
    return Monad::Result<QVector<T>>(entities);
}

// Deletes any <directory>/<subdir>/*<suffix> file whose stem is not in `keep`,
// so disk matches memory after a save. Scoped strictly to that suffix in that
// subdir — never touches anything else.
Monad::ResultBase reconcileDir(const QString &directory, const QString &subdirName,
                               const QString &suffix, const QSet<QString> &keep)
{
    QDir subdir(QDir(directory).filePath(subdirName));
    if (!subdir.exists()) {
        return Monad::ResultBase();
    }
    const QStringList files = subdir.entryList({QStringLiteral("*") + suffix}, QDir::Files);
    for (const QString &fileName : files) {
        if (keep.contains(fileName.chopped(suffix.size()))) {
            continue;
        }
        if (!subdir.remove(fileName)) {
            return Monad::ResultBase(
                QStringLiteral("%1cannot remove stale file %2")
                    .arg(kSaveErrorPrefix, subdir.filePath(fileName)));
        }
    }
    return Monad::ResultBase();
}

} // namespace

namespace cwSymbologyPaletteIO {

const QString kPaletteJsonFileName = QStringLiteral("palette.json");
const QString kBrushesSubdirName = QStringLiteral("brushes");
const QString kBrushFileSuffix = QStringLiteral(".cwbrush");
const QString kGlyphsSubdirName = QStringLiteral("glyphs");
const QString kGlyphFileSuffix = QStringLiteral(".cwglyph");

QByteArray brushToJson(const cwLineBrush &brush)
{
    CavewhereSymbologyProto::LineBrush proto;
    brushToProto(&proto, brush);
    return serializeProtoToJson(proto);
}

Monad::Result<cwLineBrush> brushFromJson(const QByteArray &json)
{
    int fileVersion = 0;
    return parseBrushJson(json, fileVersion);
}

QByteArray glyphToJson(const cwSymbologyGlyph &glyph)
{
    CavewhereSymbologyProto::Glyph proto;
    glyphToProto(&proto, glyph);
    return serializeProtoToJson(proto);
}

Monad::Result<cwSymbologyGlyph> glyphFromJson(const QByteArray &json)
{
    int fileVersion = 0;
    return parseGlyphJson(json, fileVersion);
}

QByteArray toJson(const cwSymbologyPaletteData &palette)
{
    CavewhereSymbologyProto::Palette proto;
    paletteToProto(&proto, palette);
    return serializeProtoToJson(proto);
}

Monad::Result<cwSymbologyPaletteData> fromJson(const QByteArray &json)
{
    int fileVersion = 0;
    return parsePaletteJson(json, fileVersion);
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

    QSet<QString> brushNames;
    for (const auto &brush : palette.lineBrushes) {
        const auto result = saveBrush(brush, directory);
        if (result.hasError()) {
            return result;
        }
        brushNames.insert(brush.name);
    }

    QSet<QString> glyphNames;
    for (const auto &glyph : palette.glyphs) {
        const auto result = saveGlyph(glyph, directory);
        if (result.hasError()) {
            return result;
        }
        glyphNames.insert(glyph.name);
    }

    // Disk must match memory after a save: drop files for brushes/glyphs that
    // are no longer in the palette.
    const auto brushReconcile = reconcileDir(directory, kBrushesSubdirName, kBrushFileSuffix, brushNames);
    if (brushReconcile.hasError()) {
        return brushReconcile;
    }
    return reconcileDir(directory, kGlyphsSubdirName, kGlyphFileSuffix, glyphNames);
}

Monad::ResultBase saveBrush(const cwLineBrush &brush, const QString &directory)
{
    CavewhereSymbologyProto::LineBrush proto;
    brushToProto(&proto, brush);
    const auto json = serializeJson(proto);
    if (json.hasError()) {
        return Monad::ResultBase(json.errorMessage());
    }
    return writeNamedFile(brush.name, QStringLiteral("brush"), directory,
                          kBrushesSubdirName, kBrushFileSuffix, json.value());
}

Monad::Result<cwLineBrush> loadBrush(const QString &directory, const QString &brushName)
{
    const auto contents =
        readNamedFile(directory, kBrushesSubdirName, kBrushFileSuffix, brushName, QStringLiteral("brush"));
    if (contents.hasError()) {
        return Monad::Result<cwLineBrush>(contents.errorMessage());
    }
    return brushFromJson(contents.value());
}

Monad::ResultBase saveGlyph(const cwSymbologyGlyph &glyph, const QString &directory)
{
    CavewhereSymbologyProto::Glyph proto;
    glyphToProto(&proto, glyph);
    const auto json = serializeJson(proto);
    if (json.hasError()) {
        return Monad::ResultBase(json.errorMessage());
    }
    return writeNamedFile(glyph.name, QStringLiteral("glyph"), directory,
                          kGlyphsSubdirName, kGlyphFileSuffix, json.value());
}

Monad::Result<cwSymbologyGlyph> loadGlyph(const QString &directory, const QString &glyphName)
{
    const auto contents =
        readNamedFile(directory, kGlyphsSubdirName, kGlyphFileSuffix, glyphName, QStringLiteral("glyph"));
    if (contents.hasError()) {
        return Monad::Result<cwSymbologyGlyph>(contents.errorMessage());
    }
    return glyphFromJson(contents.value());
}

Monad::ResultBase removeGlyph(const QString &directory, const QString &glyphName)
{
    return removeNamedFile(directory, kGlyphsSubdirName, kGlyphFileSuffix, glyphName,
                           QStringLiteral("glyph"));
}

Monad::Result<cwSymbologyPaletteLoadResult> load(const QString &directory)
{
    using LoadResult = Monad::Result<cwSymbologyPaletteLoadResult>;

    const QDir dir(directory);
    const QString jsonPath = dir.filePath(kPaletteJsonFileName);
    if (!QFileInfo::exists(jsonPath)) {
        return LoadResult(
            QStringLiteral("%1no %2 in %3").arg(kLoadErrorPrefix, kPaletteJsonFileName, directory));
    }

    QFile file(jsonPath);
    if (!file.open(QFile::ReadOnly)) {
        return LoadResult(
            QStringLiteral("%1cannot open %2: %3")
                .arg(kLoadErrorPrefix, jsonPath, file.errorString()));
    }
    const QByteArray contents = file.readAll();
    file.close();

    // Highest format version across palette.json + every brush/glyph file. The
    // directory-truth model lets a newer build add one file to an older palette,
    // so the gate is the max over all files, not just palette.json's stamp.
    int maxFileVersion = 0;

    int paletteVersion = 0;
    auto paletteResult = parsePaletteJson(contents, paletteVersion);
    if (paletteResult.hasError()) {
        return LoadResult(paletteResult.errorMessage());
    }
    maxFileVersion = (std::max)(maxFileVersion, paletteVersion);
    cwSymbologyPaletteData palette = paletteResult.value();

    // The directory is the source of truth: the brushes/ and glyphs/ files
    // present *are* the palette's brushes and glyphs. The per-file loaders fold
    // each file's stamped version into maxFileVersion as they parse.
    auto brushes = scanEntities<cwLineBrush>(
        directory, kBrushesSubdirName, kBrushFileSuffix, QStringLiteral("brush"),
        [&maxFileVersion](const QString &d, const QString &name) {
            const auto fileContents =
                readNamedFile(d, kBrushesSubdirName, kBrushFileSuffix, name, QStringLiteral("brush"));
            if (fileContents.hasError()) {
                return Monad::Result<cwLineBrush>(fileContents.errorMessage());
            }
            int version = 0;
            auto brush = parseBrushJson(fileContents.value(), version);
            if (!brush.hasError()) {
                maxFileVersion = (std::max)(maxFileVersion, version);
            }
            return brush;
        });
    if (brushes.hasError()) {
        return LoadResult(brushes.errorMessage());
    }
    palette.lineBrushes = brushes.value();

    auto glyphs = scanEntities<cwSymbologyGlyph>(
        directory, kGlyphsSubdirName, kGlyphFileSuffix, QStringLiteral("glyph"),
        [&maxFileVersion](const QString &d, const QString &name) {
            const auto fileContents =
                readNamedFile(d, kGlyphsSubdirName, kGlyphFileSuffix, name, QStringLiteral("glyph"));
            if (fileContents.hasError()) {
                return Monad::Result<cwSymbologyGlyph>(fileContents.errorMessage());
            }
            int version = 0;
            auto glyph = parseGlyphJson(fileContents.value(), version);
            if (!glyph.hasError()) {
                maxFileVersion = (std::max)(maxFileVersion, version);
            }
            return glyph;
        });
    if (glyphs.hasError()) {
        return LoadResult(glyphs.errorMessage());
    }
    palette.glyphs = glyphs.value();

    // With brushes and glyph strokes present, the glyph → brush → glyph graph
    // can be checked for cycles (Decision 9). A cycle would recurse forever at
    // tessellation time, so reject the palette here.
    const QString cycle = palette.findGlyphDependencyCycle();
    if (!cycle.isEmpty()) {
        return LoadResult(
            QStringLiteral("%1glyph dependency cycle: %2").arg(kLoadErrorPrefix, cycle));
    }

    // Rule-stack well-formedness (commit 4.4). With brushes and glyphs both
    // present, every decoration layer's stack is checked here: a fatal problem
    // (an ambiguous stack the engine would resolve arbitrarily) refuses the
    // palette like the cycle check above; warnings ride out in the result for the
    // app and editor to surface. Glyph references resolve against the loaded set.
    QSet<QString> glyphNames;
    glyphNames.reserve(palette.glyphs.size());
    for (const cwSymbologyGlyph &glyph : palette.glyphs) {
        glyphNames.insert(glyph.name);
    }

    const cwPlacementRuleRegistry &registry = cwPlacementRuleRegistry::instance();
    QList<cwError> warnings;
    QStringList fatalMessages;
    for (const cwLineBrush &brush : palette.lineBrushes) {
        for (int i = 0; i < brush.decorations.size(); ++i) {
            const QList<cwError> layerErrors =
                cwDecorationLayerValidator::validate(brush.decorations.at(i), registry, glyphNames);
            for (const cwError &error : layerErrors) {
                const QString located = QStringLiteral("brush \"%1\" layer %2 %3")
                                            .arg(brush.name).arg(i).arg(error.message());
                if (error.type() == cwError::Fatal) {
                    fatalMessages.append(located);
                } else {
                    cwError relocated = error;
                    relocated.setMessage(located);
                    warnings.append(relocated);
                }
            }
        }
    }
    if (!fatalMessages.isEmpty()) {
        return LoadResult(QStringLiteral("%1%2").arg(
            kLoadErrorPrefix, fatalMessages.join(QStringLiteral("; "))));
    }

    return LoadResult(cwSymbologyPaletteLoadResult{palette, maxFileVersion, warnings});
}

} // namespace cwSymbologyPaletteIO
