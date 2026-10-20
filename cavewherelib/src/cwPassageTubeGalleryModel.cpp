/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Our includes
#include "cwPassageTubeGalleryModel.h"
#include "cwCave.h"
#include "cwCavingRegion.h"
#include "cwError.h"
#include "cwErrorListModel.h"
#include "cwGlobals.h"
#include "cwLinePlotManager.h"
#include "cwPassageTubeGeometry.h"
#include "cwPassageTubeUnion.h"
#include "cwProject.h"
#include "cwRenderLinePlot.h"
#include "cwRenderPassageTube.h"
#include "cwSaveLoadPrivate.h"
#include "cwScene.h"
#include "cwStation.h"

//Qt includes
#include <QDir>
#include <QElapsedTimer>
#include <QFileInfo>
#include <QHash>
#include <QLineF>
#include <QMap>
#include <QSet>
#include <QTemporaryDir>
#include <QTimer>
#include <QVector4D>
#include <QtMath>

//Async includes
#include <asyncfuture.h>

//Std includes
#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <memory>
#include <optional>

namespace {

using Cases = cwPassageTubeCases;
using Builder = cwPassageTubeBuilder;

const QVector3D kFallbackArmDirection(1.0f, 0.0f, 0.0f);
constexpr float kMinHorizontalLength = 1e-4f;
constexpr double kFallbackRadius = 1.0;
constexpr int kJunctionDegree = 3;
constexpr float kStationMatchMeters = 1e-3f;
const QVector3D kWorldUp(0.0f, 0.0f, 1.0f);

bool isSweep(const Cases::Case& entry)
{
    return entry.tags.contains(QStringLiteral("sweep"));
}

QString lrudText(const cwStation& station)
{
    QStringList readings;
    for (const cwDistanceReading& reading : {station.left(), station.right(), station.up(), station.down()}) {
        readings.append(reading.value().isEmpty() ? QStringLiteral("-") : reading.value());
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

QVector3D horizontalDirection(const QVector3D& from, const QVector3D& to)
{
    QVector3D horizontal = to - from;
    horizontal.setZ(0.0f);
    return horizontal.length() > kMinHorizontalLength ? horizontal.normalized() : kFallbackArmDirection;
}

// The leg end nearest position, empty when there are no legs.
QString nearestStation(const Builder::Debug& debug, const QVector3D& position)
{
    QString nearest;
    float nearestDistance = std::numeric_limits<float>::max();
    const auto consider = [&](const QString& name, const QVector3D& stationPosition) {
        const float distance = position.distanceToPoint(stationPosition);
        if (distance < nearestDistance) {
            nearest = name;
            nearestDistance = distance;
        }
    };
    for (const Builder::Leg& leg : debug.legs) {
        consider(leg.from, leg.fromPosition);
        consider(leg.to, leg.toPosition);
    }
    return nearest;
}

QString issueLine(const Builder::Debug& debug, const cwPassageTubeMeshCheck::Issue& issue, const QString& prefix = QString())
{
    const QString station = nearestStation(debug, issue.position);
    const QString position = vectorText(issue.position);
    return QStringLiteral("%1%2 at %3: %4")
        .arg(prefix, cwPassageTubeMeshCheck::typeName(issue.type),
             station.isEmpty() ? position : station + QLatin1Char(' ') + position, issue.detail);
}

// The box a station's readings reach around position: left and right in plan,
// up above and down below.
QBox3D readingBox(const QVector3D& position, const cwStationLrud& lrud)
{
    const float side = static_cast<float>(std::max(lrud.left.value_or(0.0), lrud.right.value_or(0.0)));
    const float up = static_cast<float>(lrud.up.value_or(0.0));
    const float down = static_cast<float>(lrud.down.value_or(0.0));
    return QBox3D(position - QVector3D(side, side, down), position + QVector3D(side, side, up));
}

// The path of path's copy in directory: a .cwproj brings the directory it
// sits in along; empty when the copy fails.
QString copyProject(const QString& path, const QDir& directory)
{
    const QFileInfo source(path);
    if (!source.isFile()) {
        return QString();
    }
    if (source.suffix().compare(QStringLiteral("cwproj"), Qt::CaseInsensitive) == 0) {
        const QDir copy(directory.filePath(source.dir().dirName()));
        if (cwSaveLoadPrivate::copyDirectoryRecursively(source.dir(), copy).hasError()) {
            return QString();
        }
        return copy.filePath(source.fileName());
    }
    const QString copy = directory.filePath(source.fileName());
    return QFile::copy(source.absoluteFilePath(), copy) ? copy : QString();
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

// The station of the leg's two ends nearer position.
std::pair<QString, QVector3D> nearerStation(const WalkedLeg& leg, const QVector3D& position)
{
    if (position.distanceToPoint(leg.startPosition) <= position.distanceToPoint(leg.endPosition)) {
        return {leg.start, leg.startPosition};
    }
    return {leg.end, leg.endPosition};
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

// The point of bin's section loop: its station radius from the section's
// station along the bin's angle in the ring's frame.
QVector3D sectionPoint(const Builder::Ring& ring, int bin)
{
    return ring.section->station + cwPassageTubeGeometry::binDirection(ring, bin) * static_cast<float>(ring.section->stationRadii.at(bin));
}

struct Overshoot {
    int bin = 0;
    double meters = 0.0;
};

// The vertex of the ring that reaches farthest past its station's readings,
// as cwPassageTubeMeshCheck measures it.
std::optional<Overshoot> largestOvershoot(const Builder::Debug& debug, const Builder::Chain& chain, const Builder::Ring& ring)
{
    const QVector<std::optional<cwPassageTubeMeshCheck::WallReach>> reaches = cwPassageTubeMeshCheck::wallReaches(debug, chain, ring);
    std::optional<Overshoot> largest;
    for (int bin = 0; bin < reaches.size(); ++bin) {
        const std::optional<cwPassageTubeMeshCheck::WallReach>& reach = reaches.at(bin);
        if (reach && (!largest || reach->beyondMeters > largest->meters)) {
            largest = Overshoot {bin, reach->beyondMeters};
        }
    }
    return largest;
}

QString overshootText(const Builder::Ring& ring, const Overshoot& overshoot)
{
    return QStringLiteral("%1 m past the readings at %2°")
        .arg(overshoot.meters, 0, 'f', 2)
        .arg(qRadiansToDegrees(ring.binAngles.at(overshoot.bin)), 0, 'f', 0);
}

QString lrudValuesText(const cwStationLrud& lrud)
{
    QStringList values;
    for (const std::optional<double>& value : {lrud.left, lrud.right, lrud.up, lrud.down}) {
        values.append(value ? QString::number(*value, 'f', 2) : QStringLiteral("-"));
    }
    return values.join(QLatin1Char('/'));
}

// A station occurrence's readings: the walls at one end of a leg.
struct StationReadings {
    QVector3D position;
    cwStationLrud lrud;
    cwStationLrud passageLrud;
    QVector3D facing;
};

// Each station occurrence once: a station read in two chunks is two.
QVector<StationReadings> stationReadings(const Builder::Debug& debug)
{
    QSet<std::pair<std::pair<QUuid, int>, QString>> seen;
    QVector<StationReadings> readings;
    const auto add = [&](const Builder::Leg& leg, const QString& name, const StationReadings& entry) {
        const std::pair<std::pair<QUuid, int>, QString> key {{leg.tripId, leg.chunkIndex}, cwStation::canonicalKey(name)};
        if (!seen.contains(key)) {
            seen.insert(key);
            readings.append(entry);
        }
    };
    for (const Builder::Leg& leg : debug.legs) {
        add(leg, leg.from, {leg.fromPosition, leg.fromLrud, leg.fromPassageLrud, leg.fromFacing});
        add(leg, leg.to, {leg.toPosition, leg.toLrud, leg.toPassageLrud, leg.toFacing});
    }
    return readings;
}

// The rings fitted around station, with their sections.
QBox3D stationSectionsBox(const Builder::Debug& debug, const QVector3D& station)
{
    QBox3D box;
    for (const Builder::Chain& chain : debug.chains) {
        for (const Builder::Ring& ring : chain.rings) {
            if (ring.section && ring.section->station.distanceToPoint(station) <= kStationMatchMeters) {
                for (const QVector3D& vertex : ring.vertices) {
                    box.unite(vertex);
                }
                for (int bin = 0; bin < ring.section->stationRadii.size(); ++bin) {
                    box.unite(sectionPoint(ring, bin));
                }
            }
        }
    }
    return box;
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
                points << ring.center << ring.center + ring.frame.*axis * static_cast<float>(cwPassageTubeGalleryModel::kFrameAxisMeters);
            }
        }
        return Range {start, static_cast<int>(points.size()) - start};
    };
    geometry.rights = appendAxes(&Builder::Frame::right);
    geometry.ups = appendAxes(&Builder::Frame::up);

    const auto appended = [&](const auto& append) {
        const int start = static_cast<int>(points.size());
        append();
        return Range {start, static_cast<int>(points.size()) - start};
    };

    using Wall = std::optional<double> cwStationLrud::*;
    const QVector<StationReadings> readings = stationReadings(debug);
    const auto appendSpokes = [&](const auto& spokeLength) {
        for (const StationReadings& entry : readings) {
            const QVector3D right = QVector3D(entry.facing.y(), -entry.facing.x(), 0.0f).normalized();
            const std::array<std::pair<Wall, QVector3D>, 4> walls {{
                {&cwStationLrud::left, -right},
                {&cwStationLrud::right, right},
                {&cwStationLrud::up, kWorldUp},
                {&cwStationLrud::down, -kWorldUp},
            }};
            for (const auto& [wall, direction] : walls) {
                if (const std::optional<double> length = spokeLength(entry, wall)) {
                    points << entry.position << entry.position + direction * static_cast<float>(*length);
                }
            }
        }
    };
    geometry.readings = appended([&]() {
        appendSpokes([](const StationReadings& entry, Wall wall) { return entry.lrud.*wall; });
    });
    geometry.passageReadings = appended([&]() {
        appendSpokes([](const StationReadings& entry, Wall wall) {
            const std::optional<double>& passage = entry.passageLrud.*wall;
            return passage == entry.lrud.*wall ? std::optional<double>() : passage;
        });
    });

    const auto forEachSectionedRing = [&](const auto& visit) {
        for (const Builder::Chain& chain : debug.chains) {
            for (const Builder::Ring& ring : chain.rings) {
                if (ring.section) {
                    visit(ring);
                }
            }
        }
    };
    forEachSectionedRing([&](const Builder::Ring& ring) {
        geometry.sectionLoops.append(appended([&]() {
            const int binCount = static_cast<int>(ring.section->stationRadii.size());
            for (int bin = 0; bin < binCount; ++bin) {
                points << sectionPoint(ring, bin) << sectionPoint(ring, (bin + 1) % binCount);
            }
        }));
    });
    geometry.overshoots = appended([&]() {
        for (const Builder::Chain& chain : debug.chains) {
            for (const Builder::Ring& ring : chain.rings) {
                const QVector<std::optional<cwPassageTubeMeshCheck::WallReach>> reaches = cwPassageTubeMeshCheck::wallReaches(debug, chain, ring);
                for (int bin = 0; bin < reaches.size(); ++bin) {
                    const std::optional<cwPassageTubeMeshCheck::WallReach>& reach = reaches.at(bin);
                    if (reach && reach->beyondMeters > cwPassageTubeMeshCheck::kWallTolerance) {
                        points << reach->readingPoint << ring.vertices.at(bin);
                    }
                }
            }
        }
    });
    return geometry;
}

QVector<cwRenderLinePlot::TypedRange> typedRanges(const cwPassageTubeGalleryModel::RingGeometry& geometry)
{
    QVector<cwRenderLinePlot::TypedRange> ranges;
    const auto appendAll = [&](const QVector<cwLinePlotGeometry::VertexRange>& sources, quint32 type) {
        for (const cwLinePlotGeometry::VertexRange& range : sources) {
            ranges.append({range, type});
        }
    };
    appendAll(geometry.loops, cwRenderLinePlot::kRingSegment);
    appendAll({geometry.rights, geometry.ups}, cwRenderLinePlot::kFrameSegment);
    appendAll({geometry.readings, geometry.passageReadings}, cwRenderLinePlot::kReadingSegment);
    appendAll(geometry.sectionLoops, cwRenderLinePlot::kSectionSegment);
    appendAll({geometry.overshoots}, cwRenderLinePlot::kOvershootSegment);
    return ranges;
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
    m_sandbox(new cwPassageTubeSandboxModel(this)),
    m_sandboxTimer(new QTimer(this))
{
    m_sandboxTimer->setSingleShot(true);
    m_sandboxTimer->setInterval(kSandboxDebounceMilliseconds);
    connect(m_sandboxTimer, &QTimer::timeout, this, &cwPassageTubeGalleryModel::rebuildSandbox);

    connect(m_sandbox, &cwPassageTubeSandboxModel::sampleSpacingChanged, this, [this]() { m_sandboxSpacingDoublings = 0; });
    connect(m_sandbox, &cwPassageTubeSandboxModel::caseChanged, this, &cwPassageTubeGalleryModel::sandboxEdited);

    loadCatalog();
}

cwPassageTubeGalleryModel::~cwPassageTubeGalleryModel()
{
    deleteRenderObjects();

    //A project's destructor waits for its save queue, so its continuations
    //find it gone from m_loadingProjects before it dies.
    const QList<cwProject*> loading = std::exchange(m_loadingProjects, {});
    qDeleteAll(loading);
}

cwPassageTubeCases::Survey cwPassageTubeGalleryModel::Row::survey() const
{
    if (const auto* entry = std::get_if<Cases::Case>(&source)) {
        return Cases::toSurvey(*entry);
    }
    return std::get<Cases::Survey>(source);
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
        return row.name;
    case TagsRole:
        return row.tags.join(QStringLiteral(", "));
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

bool cwPassageTubeGalleryModel::readingsVisible() const
{
    return m_readingsVisible;
}

void cwPassageTubeGalleryModel::setReadingsVisible(bool readingsVisible)
{
    if (m_readingsVisible == readingsVisible) {
        return;
    }
    m_readingsVisible = readingsVisible;
    emit readingsVisibleChanged();
    applyRingVisibility();
}

bool cwPassageTubeGalleryModel::sectionsVisible() const
{
    return m_sectionsVisible;
}

void cwPassageTubeGalleryModel::setSectionsVisible(bool sectionsVisible)
{
    if (m_sectionsVisible == sectionsVisible) {
        return;
    }
    m_sectionsVisible = sectionsVisible;
    emit sectionsVisibleChanged();
    applyRingVisibility();
    if (!m_station.isEmpty()) {
        locateStation();
        emit currentChanged();
    }
}

bool cwPassageTubeGalleryModel::overshootVisible() const
{
    return m_overshootVisible;
}

void cwPassageTubeGalleryModel::setOvershootVisible(bool overshootVisible)
{
    if (m_overshootVisible == overshootVisible) {
        return;
    }
    m_overshootVisible = overshootVisible;
    emit overshootVisibleChanged();
    applyRingVisibility();
}

QString cwPassageTubeGalleryModel::station() const
{
    return m_station;
}

void cwPassageTubeGalleryModel::setStation(const QString& station)
{
    if (m_station == station) {
        return;
    }
    m_station = station;
    emit stationChanged();
    locateStation();
    emit currentChanged();
}

bool cwPassageTubeGalleryModel::rawTubesVisible() const
{
    return m_rawTubesVisible;
}

void cwPassageTubeGalleryModel::setRawTubesVisible(bool rawTubesVisible)
{
    if (m_rawTubesVisible == rawTubesVisible) {
        return;
    }
    m_rawTubesVisible = rawTubesVisible;
    emit rawTubesVisibleChanged();
    selectCurrent();
}

bool cwPassageTubeGalleryModel::capsSlid() const
{
    return m_capsSlid;
}

void cwPassageTubeGalleryModel::setCapsSlid(bool capsSlid)
{
    if (m_capsSlid == capsSlid) {
        return;
    }
    m_capsSlid = capsSlid;
    emit capsSlidChanged();
    for (const Row& row : std::as_const(m_rows)) {
        row.issueCount.reset();
        row.issueSummary.clear();
    }
    if (!m_rows.isEmpty()) {
        emit dataChanged(index(0), index(static_cast<int>(m_rows.size()) - 1), {IssueCountRole, IssueSummaryRole});
    }
    selectCurrent();
}

cwPassageTubeSandboxModel* cwPassageTubeGalleryModel::sandbox() const
{
    return m_sandbox;
}

bool cwPassageTubeGalleryModel::sandboxSelected() const
{
    return m_currentIndex == kSandboxRow;
}

QString cwPassageTubeGalleryModel::sandboxBuildText() const
{
    if (!m_sandboxBuildMilliseconds) {
        return QString();
    }
    QString text = QStringLiteral("Built in %1 ms at %2 m sample spacing").arg(*m_sandboxBuildMilliseconds).arg(m_sandboxBuildSpacing);
    if (m_sandboxBuildSpacing > m_sandbox->sampleSpacing()) {
        text += QStringLiteral(", coarsened from %1 m").arg(m_sandbox->sampleSpacing());
    }
    if (sandboxSampleSpacing() > m_sandboxBuildSpacing) {
        text += QStringLiteral("; over %1 ms, so the next build uses %2 m")
                    .arg(kSandboxBuildBudgetMilliseconds)
                    .arg(sandboxSampleSpacing());
    }
    return text;
}

bool cwPassageTubeGalleryModel::canLoadSandbox(int row) const
{
    return row != kSandboxRow && row >= 0 && row < m_rows.size() && m_rows.at(row).junctionCase.has_value();
}

bool cwPassageTubeGalleryModel::loadSandbox(int row)
{
    if (!canLoadSandbox(row)) {
        return false;
    }
    m_sandbox->load(*m_rows.at(row).junctionCase);
    m_sandboxTimer->stop();
    if (m_currentIndex == kSandboxRow) {
        selectCurrent();
    } else {
        setCurrentIndex(kSandboxRow);
    }
    return true;
}

void cwPassageTubeGalleryModel::addWideArmFork()
{
    appendRows({caseRow(Cases::wideArmFork())});
}

void cwPassageTubeGalleryModel::openProject(const QString& path, double sampleSpacing)
{
    const QString localPath = cwGlobals::convertFromURL(path);
    auto directory = std::make_shared<QTemporaryDir>();
    const QString copyPath = directory->isValid() ? copyProject(localPath, QDir(directory->path())) : QString();
    if (copyPath.isEmpty()) {
        emit projectFailed(QStringLiteral("Could not copy %1 to a temporary directory").arg(localPath));
        return;
    }

    const QString label = QFileInfo(localPath).completeBaseName();
    auto* project = new cwProject(this);
    m_loadingProjects.append(project);

    //The copy lives until the project that reads it is gone.
    connect(project, &QObject::destroyed, this, [directory]() { directory->remove(); });
    connect(project, &cwProject::loaded, this, [this, project, label, sampleSpacing]() {
        auto* linePlot = new cwLinePlotManager(project);
        linePlot->setRegion(project->cavingRegion());
        collectWhenSolved(project, linePlot, label, sampleSpacing);
    });
    connect(project->errorModel(), &cwErrorListModel::countChanged, this, [this, project]() {
        if (cwError::containsFatal(project->errorModel()->toList())) {
            failProject(project, project->errorModel()->allMessagesAsText());
        }
    });
    project->loadOrConvert(copyPath);
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
    appendRows({caseRow(entry)});
}

int cwPassageTubeGalleryModel::indexOf(const QString& name) const
{
    for (int i = 0; i < m_rows.size(); ++i) {
        if (m_rows.at(i).name == name) {
            return i;
        }
    }
    for (int i = 0; i < m_rows.size(); ++i) {
        if (m_rows.at(i).name.startsWith(name, Qt::CaseInsensitive)) {
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
    return m_rows.at(m_currentIndex).name;
}

QString cwPassageTubeGalleryModel::parameters() const
{
    if (m_currentIndex < 0) {
        return QString();
    }

    const Row& row = m_rows.at(m_currentIndex);
    QStringList lines;
    lines.append(QStringLiteral("tags: %1").arg(row.tags.join(QStringLiteral(", "))));
    lines.append(QStringLiteral("sample spacing: %1 m").arg(row.sampleSpacing));
    lines.append(QStringLiteral("junction: %1").arg(vectorText(m_junction)));
    const Cases::Survey survey = row.survey();
    for (const cwCaveData& cave : std::as_const(survey.region.caves)) {
        const cwStationPositionLookup positions = survey.positionsByCave.value(cave.id);
        for (int trip = 0; trip < cave.trips.size(); ++trip) {
            if (cave.trips.size() > 1) {
                lines.append(QStringLiteral("trip %1:").arg(trip));
            }
            const QList<cwSurveyChunkData>& chunks = cave.trips.at(trip).chunks;
            for (int chunk = 0; chunk < chunks.size(); ++chunk) {
                lines.append(QStringLiteral("chunk %1 (LRUD):").arg(chunk));
                for (const cwStation& station : chunks.at(chunk).stations) {
                    const QString position = positions.hasPosition(station.name()) ? vectorText(positions.position(station.name()))
                                                                                   : QStringLiteral("unpositioned");
                    lines.append(QStringLiteral("  %1 %2  %3").arg(station.name(), position, lrudText(station)));
                }
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
    QStringList lines {m_current.unionText,
                       QStringLiteral("%1: %2 issues, %3 components")
                           .arg(m_rawTubesVisible ? QStringLiteral("Raw tubes") : QStringLiteral("Union mesh"))
                           .arg(m_current.report.issues.size())
                           .arg(m_current.report.componentCount)};
    for (const cwPassageTubeMeshCheck::Issue& issue : std::as_const(m_current.report.issues)) {
        lines.append(issueLine(m_current.debug, issue));
    }
    for (const cwPassageTubeMeshCheck::Issue& issue : std::as_const(m_current.report.warnings)) {
        lines.append(issueLine(m_current.debug, issue, QStringLiteral("warning: ")));
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
        texts.append(issueLine(m_current.debug, issue));
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
    if (m_stationFrame) {
        return m_stationFrame->box;
    }
    if (m_bounds.isNull()) {
        const float radius = static_cast<float>(kFallbackRadius);
        const QVector3D extent(radius, radius, radius);
        return QBox3D(-extent, extent);
    }
    return m_bounds;
}

QVector3D cwPassageTubeGalleryModel::junction() const
{
    return m_stationFrame ? m_stationFrame->station : m_junction;
}

QVector3D cwPassageTubeGalleryModel::armFarEnd() const
{
    return m_stationFrame ? m_stationFrame->farEnd : m_armFarEnd;
}

QVector3D cwPassageTubeGalleryModel::armDirection() const
{
    return horizontalDirection(armFarEnd(), junction());
}

cwPassageTubeMesh cwPassageTubeGalleryModel::mesh() const
{
    return m_current.debug.mesh;
}

cwPassageTubeMeshCheck::Report cwPassageTubeGalleryModel::report() const
{
    return m_current.report;
}

QColor cwPassageTubeGalleryModel::layerColor(const QString& layer) const
{
    static const QHash<QString, quint32> layerTypes {
        {QStringLiteral("rings"), cwRenderLinePlot::kRingSegment},
        {QStringLiteral("frames"), cwRenderLinePlot::kFrameSegment},
        {QStringLiteral("readings"), cwRenderLinePlot::kReadingSegment},
        {QStringLiteral("sections"), cwRenderLinePlot::kSectionSegment},
        {QStringLiteral("overshoot"), cwRenderLinePlot::kOvershootSegment},
    };
    const auto type = layerTypes.constFind(layer);
    if (type == layerTypes.cend()) {
        return QColor();
    }
    return cwRenderLinePlot::segmentColor(*type);
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
        QString sectionText;
        if (entry.section) {
            const QString station = nearestStation(m_current.debug, entry.section->station);
            sectionText = QStringLiteral(", section L/R/U/D %1 around %2 %3")
                              .arg(lrudValuesText(entry.section->readings), station, vectorText(entry.section->station));
            if (const std::optional<Overshoot> overshoot = largestOvershoot(m_current.debug, chain, entry)) {
                sectionText += QStringLiteral(", reaches ") + overshootText(entry, *overshoot);
            }
        }

        QStringList kinds;
        if (entry.isGrown) {
            kinds.append(QStringLiteral("grown"));
        }

        return QStringLiteral("chain %1 ring %2: leg %3 (%4), %5, up %6, inclination weight %7, radius %8-%9 m%10%11")
            .arg(chainIndex)
            .arg(ring)
            .arg(entry.chainLeg)
            .arg(legText, stationText, vectorText(entry.frame.up))
            .arg(entry.frame.inclinationWeight, 0, 'f', 2)
            .arg(minRadius, 0, 'f', 2)
            .arg(maxRadius, 0, 'f', 2)
            .arg(kinds.isEmpty() ? QString() : QStringLiteral(", ") + kinds.join(QStringLiteral(", ")), sectionText);
    }
    return QString();
}

cwPassageTubeGalleryModel::Row cwPassageTubeGalleryModel::caseRow(const Cases::Case& entry)
{
    return Row {entry.name, entry.tags, entry.sampleSpacing, entry};
}

cwPassageTubeGalleryModel::Row cwPassageTubeGalleryModel::caseRow(const Cases::JunctionCase& junctionCase)
{
    Row row = caseRow(Cases::toCase(junctionCase));
    row.junctionCase = junctionCase;
    return row;
}

cwPassageTubeGalleryModel::Build cwPassageTubeGalleryModel::build(const Cases::Survey& survey, double sampleSpacing, bool slideCaps)
{
    using Check = cwPassageTubeMeshCheck;
    using Warning = cwPassageTubeMesh::Warning;
    cwPassageTubeBuilder::Options options;
    options.sampleSpacingMeters = sampleSpacing;
    options.slideCaps = slideCaps;

    Build result;
    QElapsedTimer timer;
    timer.start();
    result.debug = cwPassageTubeBuilder::buildDebug(survey.region, survey.positionsByCave, {}, options);
    const qint64 milliseconds = timer.elapsed();
    result.report = Check::check(result.debug.mesh, result.debug);

    const QVector<cwPassageTubeBuilder::CapVertex>& slid = result.debug.slidCaps;
    const auto inside = std::count_if(slid.cbegin(), slid.cend(), [](const cwPassageTubeBuilder::CapVertex& vertex) { return vertex.isInside(); });
    QStringList lines {QStringLiteral("Union in %1 components, built in %2 ms").arg(result.report.componentCount).arg(milliseconds)};
    lines.append(slideCaps ? QStringLiteral("Slid %1 of %2 cap vertices outside the passages they meet inside them").arg(inside).arg(slid.size())
                           : QStringLiteral("Cap slide off"));
    for (int c = 0; c < result.report.components.size(); ++c) {
        const Check::ComponentStats& component = result.report.components.at(c);
        lines.append(QStringLiteral("Component %1: %2 triangles, volume %3 m³, genus %4")
                         .arg(c)
                         .arg(component.triangleCount)
                         .arg(component.volume, 0, 'f', 2)
                         .arg(component.genus));
    }
    for (const Warning& warning : std::as_const(result.debug.warnings)) {
        if (warning.type == Warning::TubeRejected || warning.type == Warning::EnclosedVoid || warning.type == Warning::ChainSelfCrossing) {
            lines.append(warning.message);
        }
    }
    result.unionText = lines.join(QLatin1Char('\n'));
    return result;
}

cwPassageTubeGalleryModel::Build cwPassageTubeGalleryModel::buildRawTubes(const Cases::Survey& survey, double sampleSpacing, bool slideCaps)
{
    using Union = cwPassageTubeUnion;
    cwPassageTubeBuilder::Options options;
    options.sampleSpacingMeters = sampleSpacing;

    Build result;
    result.debug = cwPassageTubeBuilder::buildChainRings(survey.region, survey.positionsByCave, options);
    if (slideCaps) {
        result.debug.slidCaps = Union::slideCaps(result.debug);
    }

    const QVector<Union::ChainTube> tubes = Union::chainTubes(result.debug);
    QVector<cwPassageTubeMesh> meshes;
    for (const Union::ChainTube& tube : tubes) {
        meshes.append(tube.mesh);
        const int triangles = static_cast<int>(tube.mesh.indices.size() / 3);
        for (int t = 0; t < triangles; ++t) {
            result.debug.triangleChain.append(tube.chain);
            result.debug.triangleInZipBand.append(false);
            result.debug.triangleCap.append(-1);
        }
    }
    const QStringList lines {
        QStringLiteral("Raw tubes: %1 tubes of %2 chains, overlapping where they meet").arg(tubes.size()).arg(result.debug.chains.size()),
        QStringLiteral("%1 cap vertices outside the passages they meet").arg(Union::capNotch(result.debug).size())};
    result.debug.mesh = Union::merged(meshes);
    result.report = cwPassageTubeMeshCheck::check(result.debug.mesh, result.debug);
    result.unionText = lines.join(QLatin1Char('\n'));
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
    QHash<QString, Cases::JunctionCase> junctionCases;
    for (const Cases::JunctionCase& junctionCase : Cases::galleryCases() + Cases::sweepCases()) {
        junctionCases.insert(junctionCase.name, junctionCase);
    }

    beginResetModel();
    m_rows = {sandboxRow()};
    for (const Cases::Case& entry : Cases::catalog()) {
        if (m_includeSweep || !isSweep(entry)) {
            Row row = caseRow(entry);
            const auto junctionCase = junctionCases.constFind(entry.name);
            if (junctionCase != junctionCases.cend()) {
                row.junctionCase = *junctionCase;
            }
            m_rows.append(row);
        }
    }
    m_rows.append(m_addedRows);
    endResetModel();
    emit countChanged();
}

cwPassageTubeGalleryModel::Row cwPassageTubeGalleryModel::sandboxRow() const
{
    const Cases::JunctionCase junctionCase = m_sandbox->junctionCase();
    Row row = caseRow(Cases::toCase(junctionCase));
    row.name = cwPassageTubeSandboxModel::sandboxName();
    row.tags = {QStringLiteral("sandbox")};
    row.sampleSpacing = sandboxSampleSpacing();
    return row;
}

double cwPassageTubeGalleryModel::sandboxSampleSpacing() const
{
    return std::ldexp(m_sandbox->sampleSpacing(), m_sandboxSpacingDoublings);
}

void cwPassageTubeGalleryModel::sandboxEdited()
{
    m_rows.replace(kSandboxRow, sandboxRow());
    m_sandboxTimer->start();
}

void cwPassageTubeGalleryModel::rebuildSandbox()
{
    if (m_currentIndex == kSandboxRow) {
        selectCurrent();
    } else {
        const QModelIndex changed = index(kSandboxRow);
        emit dataChanged(changed, changed, {IssueCountRole, IssueSummaryRole});
    }
}

void cwPassageTubeGalleryModel::noteSandboxBuild(qint64 milliseconds, double sampleSpacing)
{
    m_sandboxBuildMilliseconds = milliseconds;
    m_sandboxBuildSpacing = sampleSpacing;
    const bool builtAtCurrentSpacing = sampleSpacing >= sandboxSampleSpacing();
    const bool canCoarsen = std::ldexp(m_sandbox->sampleSpacing(), m_sandboxSpacingDoublings + 1) <= kSandboxMaxSampleSpacingMeters;
    if (milliseconds > kSandboxBuildBudgetMilliseconds && builtAtCurrentSpacing && canCoarsen) {
        ++m_sandboxSpacingDoublings;
        m_rows.replace(kSandboxRow, sandboxRow());
    }
    emit sandboxBuildChanged();
}

void cwPassageTubeGalleryModel::appendRows(const QVector<Row>& rows)
{
    if (rows.isEmpty()) {
        return;
    }
    m_addedRows.append(rows);
    const int first = static_cast<int>(m_rows.size());
    beginInsertRows(QModelIndex(), first, first + static_cast<int>(rows.size()) - 1);
    m_rows.append(rows);
    endInsertRows();
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
    cacheIssues(row, build(row.survey(), row.sampleSpacing, m_capsSlid).report);
}

void cwPassageTubeGalleryModel::selectCurrent()
{
    if (m_currentIndex < 0) {
        m_current = Build();
        m_ringGeometry = RingGeometry();
        m_bounds = QBox3D();
        m_junction = QVector3D();
        m_armFarEnd = QVector3D();
    } else {
        const Row& row = m_rows.at(m_currentIndex);
        const Cases::Survey survey = row.survey();
        const double sampleSpacing = row.sampleSpacing;
        QElapsedTimer buildTimer;
        buildTimer.start();
        m_current = m_rawTubesVisible ? buildRawTubes(survey, sampleSpacing, m_capsSlid) : build(survey, sampleSpacing, m_capsSlid);
        const qint64 buildMilliseconds = buildTimer.elapsed();
        m_ringGeometry = ringGeometryOf(m_current.debug);

        if (!m_rawTubesVisible && !row.issueCount) {
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
        locateJunction(survey);
        if (m_currentIndex == kSandboxRow) {
            noteSandboxBuild(buildMilliseconds, sampleSpacing);
        }
    }
    locateStation();

    pushToScene();
    emit currentChanged();
}

void cwPassageTubeGalleryModel::locateJunction(const Cases::Survey& survey)
{
    QList<QStringList> chunks;
    QHash<QString, QVector3D> positions;
    for (const cwCaveData& cave : std::as_const(survey.region.caves)) {
        const cwStationPositionLookup lookup = survey.positionsByCave.value(cave.id);
        for (const cwTripData& trip : cave.trips) {
            for (const cwSurveyChunkData& chunk : trip.chunks) {
                QStringList names;
                for (const cwStation& station : chunk.stations) {
                    const QString key = cwStation::canonicalKey(station.name());
                    names.append(key);
                    if (lookup.hasPosition(key)) {
                        positions.insert(key, lookup.position(key));
                    }
                }
                chunks.append(names);
            }
        }
    }

    QHash<QString, QSet<QString>> neighbors;
    QStringList order;
    for (const QStringList& chunk : std::as_const(chunks)) {
        for (int i = 0; i < chunk.size(); ++i) {
            const QString& name = chunk.at(i);
            if (!neighbors.contains(name)) {
                order.append(name);
            }
            QSet<QString>& adjacent = neighbors[name];
            if (i > 0) {
                adjacent.insert(chunk.at(i - 1));
            }
            if (i + 1 < chunk.size()) {
                adjacent.insert(chunk.at(i + 1));
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
        for (int c = 1; c < chunks.size(); ++c) {
            const QStringList& chunk = chunks.at(c);
            if (chunk.size() < 2) {
                continue;
            }
            if (chunk.constLast() == junctionName) {
                return chunk.constFirst();
            }
            if (chunk.constFirst() == junctionName) {
                return chunk.constLast();
            }
        }
        return chunks.isEmpty() || chunks.constFirst().isEmpty() ? QString() : chunks.constFirst().constFirst();
    }();

    m_junction = positions.value(junctionName, boundsCenter());
    m_armFarEnd = positions.value(farEndName, m_junction);
}

void cwPassageTubeGalleryModel::locateStation()
{
    m_stationFrame.reset();
    if (m_station.isEmpty()) {
        return;
    }

    const QString key = cwStation::canonicalKey(m_station);
    StationFrame frame;
    float longest = -1.0f;
    for (const cwPassageTubeBuilder::Leg& leg : std::as_const(m_current.debug.legs)) {
        const bool atFrom = cwStation::canonicalKey(leg.from) == key;
        if (!atFrom && cwStation::canonicalKey(leg.to) != key) {
            continue;
        }

        frame.box.unite(readingBox(leg.fromPosition, leg.fromLrud));
        frame.box.unite(readingBox(leg.toPosition, leg.toLrud));

        const float length = leg.fromPosition.distanceToPoint(leg.toPosition);
        if (length > longest) {
            longest = length;
            frame.station = atFrom ? leg.fromPosition : leg.toPosition;
            frame.farEnd = atFrom ? leg.toPosition : leg.fromPosition;
        }
    }
    if (longest >= 0.0f) {
        if (m_sectionsVisible) {
            frame.box.unite(stationSectionsBox(m_current.debug, frame.station));
        }
        m_stationFrame = frame;
    }
}

//A run can end with the line plot dirty again, so this waits for it to settle
//clean.
void cwPassageTubeGalleryModel::collectWhenSolved(cwProject* project, cwLinePlotManager* linePlot, const QString& label, double sampleSpacing)
{
    AsyncFuture::observe(linePlot->runIfNeeded()).context(this, [this, project, linePlot, label, sampleSpacing]() {
        if (!m_loadingProjects.contains(project)) {
            return;
        }
        if (linePlot->updateState() == cwUpdatable::State::Clean) {
            addProjectRows(project, label, sampleSpacing);
        } else {
            collectWhenSolved(project, linePlot, label, sampleSpacing);
        }
    });
}

void cwPassageTubeGalleryModel::addProjectRows(cwProject* project, const QString& label, double sampleSpacing)
{
    const cwCavingRegionData region = project->cavingRegion()->data();
    QHash<QUuid, cwStationPositionLookup> positionsByCave;
    for (const cwCave* cave : project->cavingRegion()->caves()) {
        positionsByCave.insert(cave->id(), cave->stationPositionLookup());
    }

    QVector<Row> rows;
    for (const cwCaveData& cave : region.caves) {
        Cases::Survey survey;
        survey.region = region;
        survey.region.caves = {cave};
        survey.positionsByCave.insert(cave.id, positionsByCave.value(cave.id));
        rows.append(Row {QStringLiteral("%1: %2").arg(label, cave.name), {QStringLiteral("project")}, sampleSpacing, survey});
    }

    dropProject(project);
    const int firstRow = static_cast<int>(m_rows.size());
    appendRows(rows);
    emit projectOpened(firstRow, static_cast<int>(rows.size()));
}

void cwPassageTubeGalleryModel::failProject(cwProject* project, const QString& message)
{
    if (!m_loadingProjects.contains(project)) {
        return;
    }
    dropProject(project);
    emit projectFailed(message);
}

void cwPassageTubeGalleryModel::dropProject(cwProject* project)
{
    m_loadingProjects.removeOne(project);
    project->deleteLater();
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
        m_ringPlot->setGeometry(m_ringGeometry.points, typedRanges(m_ringGeometry));
        applyRingVisibility();
    }
}

void cwPassageTubeGalleryModel::applyRingVisibility()
{
    if (m_ringPlot) {
        const RingGeometry& geometry = m_ringGeometry;
        const int frameStart = geometry.rights.start;
        const int readingStart = geometry.readings.start;
        const int sectionStart = geometry.passageReadings.start + geometry.passageReadings.count;
        const int overshootStart = geometry.overshoots.start;
        m_ringPlot->setRangeVisible(0, frameStart, m_ringsVisible);
        m_ringPlot->setRangeVisible(frameStart, readingStart - frameStart, m_framesVisible);
        m_ringPlot->setRangeVisible(readingStart, sectionStart - readingStart, m_readingsVisible);
        m_ringPlot->setRangeVisible(sectionStart, overshootStart - sectionStart, m_sectionsVisible);
        m_ringPlot->setRangeVisible(overshootStart, geometry.overshoots.count, m_overshootVisible);
        m_ringPlot->setVisible(m_ringsVisible || m_framesVisible || m_readingsVisible || m_sectionsVisible || m_overshootVisible);
    }
}

void cwPassageTubeGalleryModel::deleteRenderObjects()
{
    delete m_tube;
    delete m_linePlot;
    delete m_ringPlot;
}
