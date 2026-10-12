/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Our includes
#include "cwPassageTubeGalleryModel.h"
#include "cwRenderLinePlot.h"
#include "cwRenderPassageTube.h"
#include "cwScene.h"

//Qt includes
#include <QHash>
#include <QMap>
#include <QSet>

namespace {

using Cases = cwPassageTubeCases;

const QVector3D kFallbackArmDirection(1.0f, 0.0f, 0.0f);
constexpr float kMinHorizontalLength = 1e-4f;
constexpr double kFallbackRadius = 1.0;
constexpr int kJunctionDegree = 3;

bool isSweep(const Cases::Case& entry)
{
    return entry.tags.contains(QStringLiteral("sweep"));
}

QString lrudText(const QStringList& lrud)
{
    QStringList readings;
    for (const QString& reading : lrud) {
        readings.append(reading.isEmpty() ? QStringLiteral("-") : reading);
    }
    return readings.join(QLatin1Char(' '));
}

QString vectorText(const QVector3D& vector)
{
    return QStringLiteral("(%1, %2, %3)")
        .arg(vector.x(), 0, 'f', 2)
        .arg(vector.y(), 0, 'f', 2)
        .arg(vector.z(), 0, 'f', 2);
}

} // namespace

cwPassageTubeGalleryModel::cwPassageTubeGalleryModel(QObject* parent) :
    QAbstractListModel(parent),
    m_armDirection(kFallbackArmDirection)
{
    loadCatalog();
}

cwPassageTubeGalleryModel::~cwPassageTubeGalleryModel()
{
    deleteRenderObjects();
}

int cwPassageTubeGalleryModel::rowCount(const QModelIndex& parent) const
{
    if (parent.isValid()) {
        return 0;
    }
    return static_cast<int>(m_rows.size());
}

QVariant cwPassageTubeGalleryModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() || index.row() >= m_rows.size()) {
        return QVariant();
    }

    const Row& row = m_rows.at(index.row());
    switch (role) {
    case Qt::DisplayRole:
    case NameRole:
        return row.entry.name;
    case TagsRole:
        return row.entry.tags.join(QStringLiteral(", "));
    case IssueCountRole:
        ensureIssueCount(row);
        return *row.issueCount;
    case IssueSummaryRole:
        ensureIssueCount(row);
        return row.issueSummary;
    default:
        return QVariant();
    }
}

QHash<int, QByteArray> cwPassageTubeGalleryModel::roleNames() const
{
    return {
        {NameRole, QByteArrayLiteral("name")},
        {TagsRole, QByteArrayLiteral("tags")},
        {IssueCountRole, QByteArrayLiteral("issueCount")},
        {IssueSummaryRole, QByteArrayLiteral("issueSummary")},
    };
}

int cwPassageTubeGalleryModel::count() const
{
    return rowCount();
}

bool cwPassageTubeGalleryModel::includeSweep() const
{
    return m_includeSweep;
}

void cwPassageTubeGalleryModel::setIncludeSweep(bool includeSweep)
{
    if (m_includeSweep == includeSweep) {
        return;
    }
    m_includeSweep = includeSweep;
    emit includeSweepChanged();

    setCurrentIndex(-1);
    loadCatalog();
}

int cwPassageTubeGalleryModel::currentIndex() const
{
    return m_currentIndex;
}

void cwPassageTubeGalleryModel::setCurrentIndex(int currentIndex)
{
    if (currentIndex < -1 || currentIndex >= m_rows.size()) {
        currentIndex = -1;
    }
    if (m_currentIndex == currentIndex) {
        return;
    }
    m_currentIndex = currentIndex;
    emit currentIndexChanged();
    selectCurrent();
}

cwScene* cwPassageTubeGalleryModel::scene() const
{
    return m_scene;
}

void cwPassageTubeGalleryModel::setScene(cwScene* scene)
{
    if (m_scene == scene) {
        return;
    }
    deleteRenderObjects();
    m_scene = scene;

    // Parented to the scene, so a scene that dies first takes them with it.
    if (m_scene) {
        m_tube = new cwRenderPassageTube(m_scene);
        m_linePlot = new cwRenderLinePlot(m_scene);
        m_tube->setScene(m_scene);
        m_linePlot->setScene(m_scene);
        pushToScene();
    }
    emit sceneChanged();
}

cwRenderPassageTube* cwPassageTubeGalleryModel::tube() const
{
    return m_tube;
}

cwRenderLinePlot* cwPassageTubeGalleryModel::linePlot() const
{
    return m_linePlot;
}

void cwPassageTubeGalleryModel::addRandom(int seed, int junctionCount, bool wild)
{
    Cases::Case entry;
    entry.name = QStringLiteral("%1 seed %2, %3 junctions")
                     .arg(wild ? QStringLiteral("wild random") : QStringLiteral("random"))
                     .arg(seed)
                     .arg(junctionCount);
    entry.tags = {QStringLiteral("random"),
                  QStringLiteral("seed:%1").arg(seed),
                  QStringLiteral("junctions:%1").arg(junctionCount)};
    if (wild) {
        entry.tags.append(QStringLiteral("wild"));
    }
    const auto seedValue = static_cast<unsigned int>(seed);
    entry.trip = wild ? Cases::randomWild(seedValue, junctionCount) : Cases::random(seedValue, junctionCount);
    m_randomCases.append(entry);

    const int row = static_cast<int>(m_rows.size());
    beginInsertRows(QModelIndex(), row, row);
    m_rows.append(Row {entry});
    endInsertRows();
    emit countChanged();
}

int cwPassageTubeGalleryModel::indexOf(const QString& name) const
{
    for (int i = 0; i < m_rows.size(); ++i) {
        if (m_rows.at(i).entry.name == name) {
            return i;
        }
    }
    for (int i = 0; i < m_rows.size(); ++i) {
        if (m_rows.at(i).entry.name.startsWith(name, Qt::CaseInsensitive)) {
            return i;
        }
    }
    return -1;
}

QList<int> cwPassageTubeGalleryModel::failingRows()
{
    QList<int> failing;
    for (int i = 0; i < m_rows.size(); ++i) {
        const Row& row = m_rows.at(i);
        ensureIssueCount(row);
        if (*row.issueCount > 0) {
            failing.append(i);
        }
    }
    if (!m_rows.isEmpty()) {
        emit dataChanged(index(0), index(static_cast<int>(m_rows.size()) - 1),
                         {IssueCountRole, IssueSummaryRole});
    }
    return failing;
}

QString cwPassageTubeGalleryModel::currentName() const
{
    if (m_currentIndex < 0) {
        return QString();
    }
    return m_rows.at(m_currentIndex).entry.name;
}

QString cwPassageTubeGalleryModel::parameters() const
{
    if (m_currentIndex < 0) {
        return QString();
    }

    const Cases::Case& entry = m_rows.at(m_currentIndex).entry;
    QStringList lines;
    lines.append(QStringLiteral("tags: %1").arg(entry.tags.join(QStringLiteral(", "))));
    lines.append(QStringLiteral("sample spacing: %1 m").arg(entry.sampleSpacing));
    lines.append(QStringLiteral("junction: %1").arg(vectorText(m_junction)));
    for (int chunk = 0; chunk < entry.trip.chunks.size(); ++chunk) {
        lines.append(QStringLiteral("chunk %1 (LRUD):").arg(chunk));
        for (const Cases::Station& station : entry.trip.chunks.at(chunk)) {
            const QString position = station.position ? vectorText(*station.position) : QStringLiteral("unpositioned");
            lines.append(QStringLiteral("  %1 %2  %3").arg(station.name, position, lrudText(station.lrud)));
        }
    }
    return lines.join(QLatin1Char('\n'));
}

QString cwPassageTubeGalleryModel::reportText() const
{
    if (m_currentIndex < 0) {
        return QString();
    }
    return cwPassageTubeMeshCheck::toText(m_current.report);
}

QVector<QVector3D> cwPassageTubeGalleryModel::issuePositions() const
{
    QVector<QVector3D> positions;
    positions.reserve(m_current.report.issues.size());
    for (const cwPassageTubeMeshCheck::Issue& issue : m_current.report.issues) {
        positions.append(issue.position);
    }
    return positions;
}

QStringList cwPassageTubeGalleryModel::issueTexts() const
{
    QStringList texts;
    texts.reserve(m_current.report.issues.size());
    for (const cwPassageTubeMeshCheck::Issue& issue : m_current.report.issues) {
        texts.append(QStringLiteral("%1 at %2: %3")
                         .arg(cwPassageTubeMeshCheck::typeName(issue.type), vectorText(issue.position), issue.detail));
    }
    return texts;
}

QVector3D cwPassageTubeGalleryModel::boundsCenter() const
{
    return m_bounds.isNull() ? QVector3D() : m_bounds.center();
}

double cwPassageTubeGalleryModel::boundsRadius() const
{
    if (m_bounds.isNull()) {
        return kFallbackRadius;
    }
    return 0.5 * m_bounds.size().length();
}

QBox3D cwPassageTubeGalleryModel::framingBox() const
{
    if (m_bounds.isNull()) {
        const float radius = static_cast<float>(kFallbackRadius);
        const QVector3D extent(radius, radius, radius);
        return QBox3D(-extent, extent);
    }
    return m_bounds;
}

QVector3D cwPassageTubeGalleryModel::junction() const
{
    return m_junction;
}

QVector3D cwPassageTubeGalleryModel::armFarEnd() const
{
    return m_armFarEnd;
}

QVector3D cwPassageTubeGalleryModel::armDirection() const
{
    return m_armDirection;
}

cwPassageTubeMesh cwPassageTubeGalleryModel::mesh() const
{
    return m_current.debug.mesh;
}

cwPassageTubeMeshCheck::Report cwPassageTubeGalleryModel::report() const
{
    return m_current.report;
}

cwPassageTubeGalleryModel::Build cwPassageTubeGalleryModel::build(const Cases::Case& entry)
{
    const Cases::Survey survey = Cases::toSurvey(entry.trip);
    cwPassageTubeBuilder::Options options;
    options.sampleSpacingMeters = entry.sampleSpacing;

    Build result;
    result.debug = cwPassageTubeBuilder::buildDebug(survey.region, survey.positionsByCave, {}, options);
    result.report = cwPassageTubeMeshCheck::check(result.debug.mesh, result.debug);
    return result;
}

QString cwPassageTubeGalleryModel::summary(const cwPassageTubeMeshCheck::Report& report)
{
    QMap<int, int> countsByType;
    for (const cwPassageTubeMeshCheck::Issue& issue : report.issues) {
        ++countsByType[issue.type];
    }

    QStringList parts;
    for (auto it = countsByType.cbegin(); it != countsByType.cend(); ++it) {
        const auto type = static_cast<cwPassageTubeMeshCheck::Type>(it.key());
        parts.append(QStringLiteral("%1 %2").arg(it.value()).arg(cwPassageTubeMeshCheck::typeName(type)));
    }
    return parts.join(QStringLiteral(", "));
}

void cwPassageTubeGalleryModel::loadCatalog()
{
    beginResetModel();
    m_rows.clear();
    for (const Cases::Case& entry : Cases::catalog()) {
        if (m_includeSweep || !isSweep(entry)) {
            m_rows.append(Row {entry});
        }
    }
    for (const Cases::Case& entry : std::as_const(m_randomCases)) {
        m_rows.append(Row {entry});
    }
    endResetModel();
    emit countChanged();
}

void cwPassageTubeGalleryModel::cacheIssues(const Row& row, const cwPassageTubeMeshCheck::Report& report) const
{
    row.issueCount = static_cast<int>(report.issues.size());
    row.issueSummary = summary(report);
}

void cwPassageTubeGalleryModel::ensureIssueCount(const Row& row) const
{
    if (row.issueCount) {
        return;
    }
    cacheIssues(row, build(row.entry).report);
}

void cwPassageTubeGalleryModel::selectCurrent()
{
    if (m_currentIndex < 0) {
        m_current = Build();
        m_bounds = QBox3D();
        m_junction = QVector3D();
        m_armFarEnd = QVector3D();
        m_armDirection = kFallbackArmDirection;
    } else {
        const Row& row = m_rows.at(m_currentIndex);
        m_current = build(row.entry);

        if (!row.issueCount) {
            cacheIssues(row, m_current.report);
            const QModelIndex changed = index(m_currentIndex);
            emit dataChanged(changed, changed, {IssueCountRole, IssueSummaryRole});
        }

        m_bounds = QBox3D();
        for (const QVector3D& position : std::as_const(m_current.debug.mesh.positions)) {
            m_bounds.unite(position);
        }
        for (const cwPassageTubeBuilder::Leg& leg : std::as_const(m_current.debug.legs)) {
            m_bounds.unite(leg.fromPosition);
            m_bounds.unite(leg.toPosition);
        }
        locateJunction(row.entry.trip);
    }

    pushToScene();
    emit currentChanged();
}

void cwPassageTubeGalleryModel::locateJunction(const Cases::Trip& trip)
{
    QHash<QString, QVector3D> positions;
    QHash<QString, QSet<QString>> neighbors;
    QStringList order;
    for (const QList<Cases::Station>& chunk : trip.chunks) {
        for (int i = 0; i < chunk.size(); ++i) {
            const Cases::Station& station = chunk.at(i);
            if (station.position) {
                positions.insert(station.name, *station.position);
            }
            if (!neighbors.contains(station.name)) {
                order.append(station.name);
            }
            QSet<QString>& adjacent = neighbors[station.name];
            if (i > 0) {
                adjacent.insert(chunk.at(i - 1).name);
            }
            if (i + 1 < chunk.size()) {
                adjacent.insert(chunk.at(i + 1).name);
            }
        }
    }

    const auto junctionName = [&]() -> QString {
        for (const QString& name : std::as_const(order)) {
            if (neighbors.value(name).size() >= kJunctionDegree && positions.contains(name)) {
                return name;
            }
        }
        return QString();
    }();

    // The first chunk other than the first that ends at the junction is its
    // first arm; without one, the first chunk's start stands in for it.
    const auto farEndName = [&]() -> QString {
        for (int c = 1; c < trip.chunks.size(); ++c) {
            const QList<Cases::Station>& chunk = trip.chunks.at(c);
            if (chunk.size() < 2) {
                continue;
            }
            if (chunk.constLast().name == junctionName) {
                return chunk.constFirst().name;
            }
            if (chunk.constFirst().name == junctionName) {
                return chunk.constLast().name;
            }
        }
        return trip.chunks.isEmpty() || trip.chunks.constFirst().isEmpty()
                   ? QString()
                   : trip.chunks.constFirst().constFirst().name;
    }();

    m_junction = positions.value(junctionName, boundsCenter());
    m_armFarEnd = positions.value(farEndName, m_junction);

    QVector3D horizontal = m_junction - m_armFarEnd;
    horizontal.setZ(0.0f);
    m_armDirection = horizontal.length() > kMinHorizontalLength ? horizontal.normalized()
                                                                 : kFallbackArmDirection;
}

void cwPassageTubeGalleryModel::pushToScene()
{
    if (m_tube) {
        m_tube->setMesh(m_current.debug.mesh);
    }
    if (m_linePlot) {
        QVector<QVector3D> points;
        points.reserve(m_current.debug.legs.size() * 2);
        for (const cwPassageTubeBuilder::Leg& leg : std::as_const(m_current.debug.legs)) {
            points.append(leg.fromPosition);
            points.append(leg.toPosition);
        }
        m_linePlot->setGeometry(points);
    }
}

void cwPassageTubeGalleryModel::deleteRenderObjects()
{
    delete m_tube;
    delete m_linePlot;
}
