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
#include <QSet>
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

// palette.json carries palette-level identity only; brushes and glyphs are
// per-file (the directory is their source of truth), so they never travel here.
void paletteToProto(CavewhereSymbologyProto::Palette *proto, const cwSymbologyPaletteData &palette)
{
    proto->set_id(toStd(palette.id.toString(QUuid::WithoutBraces)));
    proto->set_name(toStd(palette.name));
    proto->set_description(toStd(palette.description));
    proto->set_author(toStd(palette.author));
    proto->set_version(toStd(palette.version));
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
    CavewhereSymbologyProto::LineBrush proto;
    const auto status = google::protobuf::util::JsonStringToMessage(
        std::string(json.constData(), static_cast<size_t>(json.size())), &proto);
    if (!status.ok()) {
        return Monad::Result<cwLineBrush>(
            QStringLiteral("%1brush JSON parse error: %2")
                .arg(kLoadErrorPrefix, QString::fromUtf8(status.ToString().c_str())));
    }
    return Monad::Result<cwLineBrush>(brushFromProto(proto));
}

QByteArray glyphToJson(const cwSymbologyGlyph &glyph)
{
    CavewhereSymbologyProto::Glyph proto;
    glyphToProto(&proto, glyph);
    return serializeProtoToJson(proto);
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
    return serializeProtoToJson(proto);
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
    cwSymbologyPaletteData palette = paletteResult.value();

    // The directory is the source of truth: the brushes/ and glyphs/ files
    // present *are* the palette's brushes and glyphs.
    auto brushes = scanEntities<cwLineBrush>(directory, kBrushesSubdirName, kBrushFileSuffix,
                                             QStringLiteral("brush"), loadBrush);
    if (brushes.hasError()) {
        return Monad::Result<cwSymbologyPaletteData>(brushes.errorMessage());
    }
    palette.lineBrushes = brushes.value();

    auto glyphs = scanEntities<cwSymbologyGlyph>(directory, kGlyphsSubdirName, kGlyphFileSuffix,
                                                 QStringLiteral("glyph"), loadGlyph);
    if (glyphs.hasError()) {
        return Monad::Result<cwSymbologyPaletteData>(glyphs.errorMessage());
    }
    palette.glyphs = glyphs.value();

    // With brushes and glyph strokes present, the glyph → brush → glyph graph
    // can be checked for cycles (Decision 9). A cycle would recurse forever at
    // tessellation time, so reject the palette here.
    const QString cycle = palette.findGlyphDependencyCycle();
    if (!cycle.isEmpty()) {
        return Monad::Result<cwSymbologyPaletteData>(
            QStringLiteral("%1glyph dependency cycle: %2").arg(kLoadErrorPrefix, cycle));
    }

    return Monad::Result<cwSymbologyPaletteData>(palette);
}

} // namespace cwSymbologyPaletteIO
