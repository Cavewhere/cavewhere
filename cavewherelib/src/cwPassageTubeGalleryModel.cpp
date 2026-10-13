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
#include <QLineF>
#include <QMap>
#include <QSet>
#include <QVector4D>
#include <QtMath>

//Std includes
#include <algorithm>
#include <cmath>
#include <limits>
#include <optional>

namespace {

using Cases = cwPassageTubeCases;
using Builder = cwPassageTubeBuilder;

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

// A chain leg's stations in the order the chain walks them.
struct WalkedLeg {
    QString start;
    QString end;
    QVector3D startPosition;
    QVector3D endPosition;
};

std::optional<WalkedLeg> walkedLeg(const Builder::Debug& debug, const Builder::Chain& chain, int chainLeg)
{
    if (chainLeg < 0 || chainLeg >= chain.legs.size()) {
        return std::nullopt;
    }
    const Builder::ChainLeg& walked = chain.legs.at(chainLeg);
    if (walked.legIndex < 0 || walked.legIndex >= debug.legs.size()) {
        return std::nullopt;
    }
    const Builder::Leg& leg = debug.legs.at(walked.legIndex);
    if (walked.reversed) {
        return WalkedLeg {leg.to, leg.from, leg.toPosition, leg.fromPosition};
    }
    return WalkedLeg {leg.from, leg.to, leg.fromPosition, leg.toPosition};
}

double angleDegrees(const QVector3D& a, const QVector3D& b)
{
    const double cosine = QVector3D::dotProduct(a.normalized(), b.normalized());
    return qRadiansToDegrees(std::acos(std::clamp(cosine, -1.0, 1.0)));
}

// The station of the leg's two ends nearer position.
std::pair<QString, QVector3D> nearerStation(const WalkedLeg& leg, const QVector3D& position)
{
    if (position.distanceToPoint(leg.startPosition) <= position.distanceToPoint(leg.endPosition)) {
        return {leg.start, leg.startPosition};
    }
    return {leg.end, leg.endPosition};
}

// One line per run of elbow rings: its corner station, the turn there, how
// far the middle ring sits off the station and the roll of the middle
// ring's frame against each neighbor's.
QStringList elbowLines(const Builder::Debug& debug)
{
    QStringList lines;
    for (int c = 0; c < debug.chains.size(); ++c) {
        const Builder::Chain& chain = debug.chains.at(c);
        const QVector<Builder::Ring>& rings = chain.rings;
        int first = 0;
        while (first < rings.size()) {
            if (!rings.at(first).isElbow) {
                ++first;
                continue;
            }
            int last = first;
            while (last + 1 < rings.size() && rings.at(last + 1).isElbow) {
                ++last;
            }

            // The corner is the leg end nearest the run's middle ring.
            const QVector3D runMiddle = rings.at((first + last) / 2).center;
            QString station;
            QVector3D stationPosition;
            float stationDistance = std::numeric_limits<float>::max();
            QSet<int> runLegs;
            for (int r = first; r <= last; ++r) {
                runLegs.insert(rings.at(r).chainLeg);
                if (const auto leg = walkedLeg(debug, chain, rings.at(r).chainLeg)) {
                    const auto [name, position] = nearerStation(*leg, runMiddle);
                    const float distance = runMiddle.distanceToPoint(position);
                    if (distance < stationDistance) {
                        station = name;
                        stationPosition = position;
                        stationDistance = distance;
                    }
                }
            }

            int middle = first;
            for (int r = first; r <= last; ++r) {
                if (rings.at(r).center.distanceToPoint(stationPosition) < rings.at(middle).center.distanceToPoint(stationPosition)) {
                    middle = r;
                }
            }

            const auto turn = [&]() -> double {
                for (int legIndex : std::as_const(runLegs)) {
                    const auto incoming = walkedLeg(debug, chain, legIndex);
                    if (!incoming) {
                        continue;
                    }
                    const auto outgoing = walkedLeg(debug, chain, (legIndex + 1) % static_cast<int>(chain.legs.size()));
                    if (outgoing && incoming->end == station && outgoing->start == station) {
                        return angleDegrees(incoming->endPosition - incoming->startPosition,
                                            outgoing->endPosition - outgoing->startPosition);
                    }
                }
                return angleDegrees(rings.at(first).frame.tangent, rings.at(last).frame.tangent);
            }();

            const QVector3D& up = rings.at(middle).frame.up;
            const auto rollText = [&](int neighbor) {
                if (neighbor < 0 || neighbor >= rings.size()) {
                    return QStringLiteral("-");
                }
                return QStringLiteral("%1°").arg(angleDegrees(up, rings.at(neighbor).frame.up), 0, 'f', 1);
            };

            lines.append(QStringLiteral("elbow at %1 (chain %2, rings %3-%4): turn %5°, middle ring %6 off by %7 m, roll %8 before, %9 after")
                             .arg(station)
                             .arg(c)
                             .arg(first)
                             .arg(last)
                             .arg(turn, 0, 'f', 1)
                             .arg(middle)
                             .arg(rings.at(middle).center.distanceToPoint(stationPosition), 0, 'f', 2)
                             .arg(rollText(middle - 1), rollText(middle + 1)));
            first = last + 1;
        }
    }
    return lines;
}

// The index, numbered across chains in chain order, of the ring whose center
// has the smallest distance below maxDistance; -1 when none does. distanceTo
// returns nullopt for a center it skips.
template<typename DistanceTo>
int nearestRingIndex(const Builder::Debug& debug, double maxDistance, DistanceTo distanceTo)
{
    int nearest = -1;
    double nearestDistance = maxDistance;
    int ring = 0;
    for (const Builder::Chain& chain : debug.chains) {
        for (const Builder::Ring& candidate : chain.rings) {
            const std::optional<double> distance = distanceTo(candidate.center);
            if (distance && *distance < nearestDistance) {
                nearest = ring;
                nearestDistance = *distance;
            }
            ++ring;
        }
    }
    return nearest;
}

cwPassageTubeGalleryModel::RingGeometry ringGeometryOf(const Builder::Debug& debug)
{
    using Range = cwLinePlotGeometry::VertexRange;

    cwPassageTubeGalleryModel::RingGeometry geometry;
    QVector<QVector3D>& points = geometry.points;
    for (const Builder::Chain& chain : debug.chains) {
        for (const Builder::Ring& ring : chain.rings) {
            const QVector<QVector3D>& vertices = ring.vertices;
            const int start = static_cast<int>(points.size());
            const int edgeStep = ring.isGrown ? 2 : 1;
            for (int k = 0; k < vertices.size(); k += edgeStep) {
                points << vertices.at(k) << vertices.at((k + 1) % vertices.size());
            }
            geometry.loops.append(Range {start, static_cast<int>(points.size()) - start});
        }
    }

    const auto appendAxes = [&](QVector3D Builder::Frame::*axis) {
        const int start = static_cast<int>(points.size());
        for (const Builder::Chain& chain : debug.chains) {
            for (const Builder::Ring& ring : chain.rings) {
                const double reach = cwPassageTubeGalleryModel::kFrameAxisMeters
                                     * (ring.isElbow ? cwPassageTubeGalleryModel::kElbowFrameScale : 1.0);
                points << ring.center << ring.center + ring.frame.*axis * static_cast<float>(reach);
            }
        }
        return Range {start, static_cast<int>(points.size()) - start};
    };
    geometry.rights = appendAxes(&Builder::Frame::right);
    geometry.ups = appendAxes(&Builder::Frame::up);
    return geometry;
}

QVector<QVector3D> positionsOf(const QVector<cwPassageTubeMeshCheck::Issue>& issues)
{
    QVector<QVector3D> positions;
    positions.reserve(issues.size());
    for (const cwPassageTubeMeshCheck::Issue& issue : issues) {
        positions.append(issue.position);
    }
    return positions;
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
        m_ringPlot = new cwRenderLinePlot(m_scene);
        m_tube->setScene(m_scene);
        m_linePlot->setScene(m_scene);
        m_ringPlot->setScene(m_scene);
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

cwRenderLinePlot* cwPassageTubeGalleryModel::ringPlot() const
{
    return m_ringPlot;
}

bool cwPassageTubeGalleryModel::ringsVisible() const
{
    return m_ringsVisible;
}

void cwPassageTubeGalleryModel::setRingsVisible(bool ringsVisible)
{
    if (m_ringsVisible == ringsVisible) {
        return;
    }
    m_ringsVisible = ringsVisible;
    emit ringsVisibleChanged();
    applyRingVisibility();
}

bool cwPassageTubeGalleryModel::framesVisible() const
{
    return m_framesVisible;
}

void cwPassageTubeGalleryModel::setFramesVisible(bool framesVisible)
{
    if (m_framesVisible == framesVisible) {
        return;
    }
    m_framesVisible = framesVisible;
    emit framesVisibleChanged();
    applyRingVisibility();
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
    const QList<Cases::Trip> trips = entry.trips();
    for (int trip = 0; trip < trips.size(); ++trip) {
        if (trips.size() > 1) {
            lines.append(QStringLiteral("trip %1:").arg(trip));
        }
        const QList<QList<Cases::Station>>& chunks = trips.at(trip).chunks;
        for (int chunk = 0; chunk < chunks.size(); ++chunk) {
            lines.append(QStringLiteral("chunk %1 (LRUD):").arg(chunk));
            for (const Cases::Station& station : chunks.at(chunk)) {
                const QString position = station.position ? vectorText(*station.position) : QStringLiteral("unpositioned");
                lines.append(QStringLiteral("  %1 %2  %3").arg(station.name, position, lrudText(station.lrud)));
            }
        }
    }
    return lines.join(QLatin1Char('\n'));
}

QString cwPassageTubeGalleryModel::reportText() const
{
    if (m_currentIndex < 0) {
        return QString();
    }
    QStringList lines = elbowLines(m_current.debug);
    const QString checkText = cwPassageTubeMeshCheck::toText(m_current.report);
    if (!checkText.isEmpty()) {
        lines.prepend(checkText);
    }
    return lines.join(QLatin1Char('\n'));
}

QVector<QVector3D> cwPassageTubeGalleryModel::issuePositions() const
{
    return positionsOf(m_current.report.issues);
}

QVector<QVector3D> cwPassageTubeGalleryModel::warningPositions() const
{
    return positionsOf(m_current.report.warnings);
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

cwPassageTubeGalleryModel::RingGeometry cwPassageTubeGalleryModel::ringGeometry() const
{
    return m_ringGeometry;
}

int cwPassageTubeGalleryModel::ringAt(QPointF point, const QMatrix4x4& modelToViewport, double maxPixels) const
{
    return nearestRingIndex(m_current.debug, maxPixels, [&](const QVector3D& center) -> std::optional<double> {
        const QVector4D clip = modelToViewport * QVector4D(center, 1.0f);
        if (clip.w() > 0.0f) {
            return QLineF(point, clip.toVector2DAffine().toPointF()).length();
        }
        return std::nullopt;
    });
}

int cwPassageTubeGalleryModel::nearestRing(const QVector3D& position) const
{
    return nearestRingIndex(m_current.debug, std::numeric_limits<double>::max(), [&](const QVector3D& center) -> std::optional<double> {
        return position.distanceToPoint(center);
    });
}

QString cwPassageTubeGalleryModel::ringLine(int ring) const
{
    if (ring < 0) {
        return QString();
    }
    int chainIndex = 0;
    for (const Builder::Chain& chain : std::as_const(m_current.debug.chains)) {
        if (ring >= chain.rings.size()) {
            ring -= static_cast<int>(chain.rings.size());
            ++chainIndex;
            continue;
        }

        const Builder::Ring& entry = chain.rings.at(ring);
        QString legText = QStringLiteral("-");
        QString stationText = QStringLiteral("-");
        if (const auto leg = walkedLeg(m_current.debug, chain, entry.chainLeg)) {
            legText = QStringLiteral("%1-%2").arg(leg->start, leg->end);
            const auto [station, position] = nearerStation(*leg, entry.center);
            stationText = QStringLiteral("%1 m from %2").arg(entry.center.distanceToPoint(position), 0, 'f', 2).arg(station);
        }

        const auto [minRadius, maxRadius] = entry.radii.isEmpty()
                                                ? std::pair<double, double>(0.0, 0.0)
                                                : std::pair<double, double>(*std::min_element(entry.radii.cbegin(), entry.radii.cend()),
                                                                            *std::max_element(entry.radii.cbegin(), entry.radii.cend()));
        QStringList kinds;
        if (entry.isElbow) {
            kinds.append(QStringLiteral("elbow"));
        }
        if (entry.isGrown) {
            kinds.append(QStringLiteral("grown"));
        }

        return QStringLiteral("chain %1 ring %2: leg %3 (%4), %5, up %6, inclination weight %7, radius %8-%9 m%10")
            .arg(chainIndex)
            .arg(ring)
            .arg(entry.chainLeg)
            .arg(legText, stationText, vectorText(entry.frame.up))
            .arg(entry.frame.inclinationWeight, 0, 'f', 2)
            .arg(minRadius, 0, 'f', 2)
            .arg(maxRadius, 0, 'f', 2)
            .arg(kinds.isEmpty() ? QString() : QStringLiteral(", ") + kinds.join(QStringLiteral(", ")));
    }
    return QString();
}

cwPassageTubeGalleryModel::Build cwPassageTubeGalleryModel::build(const Cases::Case& entry)
{
    const Cases::Survey survey = Cases::toSurvey(entry);
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
        m_ringGeometry = RingGeometry();
        m_bounds = QBox3D();
        m_junction = QVector3D();
        m_armFarEnd = QVector3D();
        m_armDirection = kFallbackArmDirection;
    } else {
        const Row& row = m_rows.at(m_currentIndex);
        m_current = build(row.entry);
        m_ringGeometry = ringGeometryOf(m_current.debug);

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
    if (m_ringPlot) {
        m_ringPlot->setGeometry(m_ringGeometry.points, {m_ringGeometry.ups});
        applyRingVisibility();
    }
}

void cwPassageTubeGalleryModel::applyRingVisibility()
{
    if (m_ringPlot) {
        const int frameStart = m_ringGeometry.rights.start;
        const int frameCount = m_ringGeometry.rights.count + m_ringGeometry.ups.count;
        m_ringPlot->setRangeVisible(0, frameStart, m_ringsVisible);
        m_ringPlot->setRangeVisible(frameStart, frameCount, m_framesVisible);
        m_ringPlot->setVisible(m_ringsVisible || m_framesVisible);
    }
}

void cwPassageTubeGalleryModel::deleteRenderObjects()
{
    delete m_tube;
    delete m_linePlot;
    delete m_ringPlot;
}
