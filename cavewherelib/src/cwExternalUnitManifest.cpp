/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Our includes
#include "cwExternalUnitManifest.h"
#include "cwStation.h"
#include "cwCavernNaming.h"
#include "cwExternalCenterlineAttach.h"

//Qt includes
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMetaEnum>
#include <QSaveFile>
#include <QSet>

namespace {

constexpr int kManifestVersion = 1;

const QString kVersionKey = QStringLiteral("version");
const QString kRowsKey = QStringLiteral("rows");
const QString kSourcePathKey = QStringLiteral("sourcePath");
const QString kKindKey = QStringLiteral("kind");
const QString kDisplayNameKey = QStringLiteral("displayName");
const QString kCavernLabelKey = QStringLiteral("cavernLabel");
const QString kDepthKey = QStringLiteral("depth");
const QString kNodeKey = QStringLiteral("node");
const QString kUnitKey = QStringLiteral("unit");
const QString kHasSurveyContentKey = QStringLiteral("hasSurveyContent");
const QString kMissingKey = QStringLiteral("missing");
const QString kStationCountKey = QStringLiteral("stationCount");
const QString kDateKey = QStringLiteral("date");
const QString kStationsKey = QStringLiteral("stations");

QStringList canonicalStations(const QStringList& stations)
{
    QSet<QString> canonical;
    for (const QString& station : stations) {
        canonical.insert(cwStation::canonicalKey(station));
    }
    QStringList sorted(canonical.cbegin(), canonical.cend());
    sorted.sort();
    return sorted;
}

QJsonObject rowToJson(const cwScanRow& row)
{
    const QMetaEnum kinds = QMetaEnum::fromType<cwSurveyNodeKind::Kind>();

    QJsonObject object;
    object.insert(kSourcePathKey, row.sourcePath);
    object.insert(kKindKey, QString::fromLatin1(kinds.valueToKey(static_cast<int>(row.kind))));
    object.insert(kDisplayNameKey, row.displayName);
    object.insert(kCavernLabelKey, row.cavernLabel);
    object.insert(kDepthKey, row.depth);
    object.insert(kNodeKey, row.isNode);
    object.insert(kUnitKey, row.isUnit);
    object.insert(kHasSurveyContentKey, row.hasSurveyContent);
    object.insert(kMissingKey, row.missing);
    object.insert(kStationCountKey, row.stationCount);
    if (row.date.isValid()) {
        object.insert(kDateKey, row.date.toString(Qt::ISODate));
    }
    if (row.isUnit) {
        object.insert(kStationsKey, QJsonArray::fromStringList(canonicalStations(row.stations)));
    }
    return object;
}

std::optional<cwScanRow> rowFromJson(const QJsonObject& object)
{
    const QMetaEnum kinds = QMetaEnum::fromType<cwSurveyNodeKind::Kind>();
    bool knownKind = false;
    const int kind = kinds.keyToValue(object.value(kKindKey).toString().toLatin1().constData(),
                                      &knownKind);
    if (!knownKind) {
        return std::nullopt;
    }

    cwScanRow row;
    row.kind = static_cast<cwSurveyNodeKind::Kind>(kind);
    row.sourcePath = object.value(kSourcePathKey).toString();
    row.displayName = object.value(kDisplayNameKey).toString();
    row.cavernLabel = object.value(kCavernLabelKey).toString();
    row.depth = object.value(kDepthKey).toInt();
    row.isNode = object.value(kNodeKey).toBool();
    row.isUnit = object.value(kUnitKey).toBool();
    row.hasSurveyContent = object.value(kHasSurveyContentKey).toBool();
    row.missing = object.value(kMissingKey).toBool();
    row.stationCount = object.value(kStationCountKey).toInt();
    row.date = QDate::fromString(object.value(kDateKey).toString(), Qt::ISODate);
    const QJsonArray stations = object.value(kStationsKey).toArray();
    for (const QJsonValue& station : stations) {
        row.stations.append(station.toString());
    }
    return row;
}

} // namespace

namespace cwExternalUnitManifest {

QString fileName()
{
    return QStringLiteral("units.json");
}

QByteArray toJson(const QList<cwScanRow>& rows)
{
    QJsonArray rowArray;
    for (const cwScanRow& row : rows) {
        rowArray.append(rowToJson(row));
    }
    QJsonObject root;
    root.insert(kVersionKey, kManifestVersion);
    root.insert(kRowsKey, rowArray);
    return QJsonDocument(root).toJson(QJsonDocument::Indented);
}

Monad::Result<QList<cwScanRow>> fromJson(const QByteArray& json)
{
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(json, &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        return Monad::Result<QList<cwScanRow>>(
            QStringLiteral("units manifest is not a JSON object: %1").arg(parseError.errorString()));
    }

    const QJsonObject root = document.object();
    const int version = root.value(kVersionKey).toInt();
    if (version != kManifestVersion) {
        return Monad::Result<QList<cwScanRow>>(
            QStringLiteral("units manifest has version %1, this build reads %2")
                .arg(version).arg(kManifestVersion));
    }

    QList<cwScanRow> rows;
    const QJsonArray rowArray = root.value(kRowsKey).toArray();
    for (const QJsonValue& value : rowArray) {
        const std::optional<cwScanRow> row = rowFromJson(value.toObject());
        if (!row.has_value()) {
            return Monad::Result<QList<cwScanRow>>(
                QStringLiteral("units manifest names an unknown kind: %1")
                    .arg(value.toObject().value(kKindKey).toString()));
        }
        rows.append(row.value());
    }
    return Monad::Result<QList<cwScanRow>>(rows);
}

Monad::ResultBase write(const QString& path, const QList<cwScanRow>& rows)
{
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        return Monad::ResultBase(QStringLiteral("cannot write %1: %2").arg(path, file.errorString()));
    }
    file.write(toJson(rows));
    if (!file.commit()) {
        return Monad::ResultBase(QStringLiteral("cannot write %1: %2").arg(path, file.errorString()));
    }
    return Monad::ResultBase();
}

Monad::Result<QList<cwScanRow>> read(const QString& path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return Monad::Result<QList<cwScanRow>>(
            QStringLiteral("cannot read %1: %2").arg(path, file.errorString()));
    }
    return fromJson(file.readAll());
}

SolveView solveView(const QList<cwScanRow>& rows, bool preserveCase)
{
    SolveView view;

    // Pre-order, so one cavern path per depth: a row's is its parent's plus
    // its own label.
    QStringList cavernPathAtDepth;
    for (const cwScanRow& row : rows) {
        cavernPathAtDepth.resize(row.depth + 1);
        const QString parentPath = row.depth == 0 ? QString() : cavernPathAtDepth.at(row.depth - 1);
        const QString rowPath =
            parentPath + cwCavernNaming::scopePrefix(cwStation::canonicalKey(row.cavernLabel));
        cavernPathAtDepth[row.depth] = rowPath;

        if (row.depth > 0) {
            if (row.isNode) {
                view.cavernLabels.insert(row.sourcePath, row.cavernLabel);
            } else if (!row.cavernLabel.isEmpty()) {
                view.tripCavernLabels.insert(row.sourcePath, row.cavernLabel);
            }
        }
        if (!row.isUnit) {
            continue;
        }

        // A node that is also a unit holds its own trip, so the trip's node
        // path is the row's. A unit-only row's trip sits in its parent node,
        // below the trip's own label when the row has one (a leaf Survex
        // block), so either way a station is named relative to its trip.
        QSet<QString> seen;
        QStringList sorted;
        for (const QString& station : row.stations) {
            const QString key = cwStation::canonicalKey(station);
            const QString stored = preserveCase ? station : key;
            if (!key.startsWith(rowPath) || seen.contains(stored)) {
                continue;
            }
            seen.insert(stored);
            sorted.append(stored.sliced(rowPath.size()));
        }
        sorted.sort();
        const QString unitSourcePath = cwExternalCenterlineAttach::unitSourcePath(row);
        view.unitStations.insert(unitSourcePath, sorted);
        view.unitDeclinations.insert(unitSourcePath, row.declination);
    }
    return view;
}

} // namespace cwExternalUnitManifest
