/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#include "cwLazLayer.h"
#include "cwRestarterTracking.h"

//Qt includes
#include <QDebug>
#include <QFileInfo>
#include <QLoggingCategory>

//Our includes
#include "cwConcurrent.h"
#include "cwCoordinateTransform.h"
#include "cwFuture.h"
#include "cwKeyword.h"
#include "cwKeywordModel.h"

Q_LOGGING_CATEGORY(lcLazLayer, "cw.laz.layer")

namespace {
constexpr const char* kLazLayerType = "LAZ Layer";

cwGeoPoint midpoint(const cwGeoPoint& a, const cwGeoPoint& b)
{
    return cwGeoPoint((a.x + b.x) * 0.5, (a.y + b.y) * 0.5, (a.z + b.z) * 0.5);
}
}

cwLazLayer::cwLazLayer(QObject* parent) :
    QObject(parent),
    m_id(QUuid::createUuid()),
    m_keywordModel(new cwKeywordModel(this)),
    m_loadRestarter(this),
    m_probeRestarter(this)
{
    updateTypeKeyword();
    updateIdKeyword();

    // Each restart() begins a new build run; register that run with the global
    // future manager and chain the result delivery onto it. Restarter cancels
    // the previous future and waits for it to settle before this fires again,
    // so we can never have two applyResult chains racing each other. Only a
    // real build is registered: a cache hit never reaches the restarter, so
    // reopening a project shows no job at all.
    cwTrackRestarter(m_futureManagerToken, m_loadRestarter,
        [this]() { return QStringLiteral("Building point cloud octree: %1").arg(m_name); });

    // Fires once per build run, only for a completed (non-cancelled, non-empty)
    // future. cwPointOctreeBuilder always addResult()s except on cancellation —
    // a header open failure still yields an error Result — so applyResult()
    // owns all error reporting (its hasError() branch).
    m_loadRestarter.onResult(this, [this](cwPointOctreeBuilder::Result result) {
        // The user may have disabled the layer between the build start and its
        // delivery. Drop the result — setEnabled(false) already cleared the
        // octree and reset status to Idle.
        if (!m_enabled) {
            return;
        }
        if (m_buildGeneration != m_reloadGeneration) {
            // A later reload() superseded this build, so its octree is in a
            // frame or a CS the layer has moved on from. The newer run is the
            // one that publishes.
            return;
        }
        applyResult(result);
    });

    // Deliberately not registered with the future manager: a header open is
    // microseconds, and a directory of tiles would flash a progress row per
    // file for work no one waits on.
    m_probeRestarter.onResult(this, [this](cwLazLoader::ProbeResult probe) {
        finishProbe(probe);
    });
}

cwLazLayer::~cwLazLayer() = default;

cwLazLayerData cwLazLayer::data() const
{
    cwLazLayerData out;
    out.fileName = QFileInfo(m_sourcePath).fileName();
    out.id = m_id;
    out.enabled = m_enabled;
    return out;
}

void cwLazLayer::setData(const cwLazLayerData& data)
{
    // Adopt the persisted UUID. Null means the .cwlaz had no id field, so
    // keep the auto-generated one this layer was constructed with.
    if (!data.id.isNull() && m_id != data.id) {
        m_id = data.id;
        updateIdKeyword();
    }
    setEnabled(data.enabled);
}

void cwLazLayer::setSourcePath(const QString& path)
{
    // Snapshot the file's size+mtime before the path-change short-circuit:
    // a same-path reset (the same caller pointing at the same file after an
    // in-place overwrite) still needs to refresh the cached fingerprint so
    // cwLazLayerModel::rescan can detect the change. QFileInfo::size() and
    // lastModified() already sentinel to -1 / invalid for missing files.
    const QFileInfo info(path);
    const qint64 newSize = info.size();
    const QDateTime newMtime = info.lastModified();

    const bool pathChanged = (m_sourcePath != path);
    const bool fingerprintChanged =
            (m_sourceSize != newSize || m_sourceMtime != newMtime);
    if (!pathChanged && !fingerprintChanged) {
        return;
    }

    m_sourcePath = path;
    m_sourceSize = newSize;
    m_sourceMtime = newMtime;

    if (pathChanged) {
        emit sourcePathChanged();

        const QString basename = info.baseName();
        if (m_name != basename) {
            m_name = basename;
            emit nameChanged();
            updateNameKeyword();
        }
        updateFileNameKeyword();
    }

    startHeaderProbe();
    reload();
}

void cwLazLayer::renameSourcePath(const QString& newPath)
{
    if (m_sourcePath == newPath) {
        return;
    }

    const QFileInfo info(newPath);
    m_sourcePath = newPath;
    // Capture the fingerprint of the new path so the next rescan doesn't see
    // a size/mtime mismatch and force a reload of unchanged bytes. Use
    // QFileInfo::exists() to keep -1/invalid sentinels intact for files
    // that don't exist on disk yet (the Move job hasn't run when this is
    // called during rename).
    m_sourceSize = info.exists() ? info.size() : qint64{-1};
    m_sourceMtime = info.exists() ? info.lastModified() : QDateTime{};

    emit sourcePathChanged();

    const QString basename = info.baseName();
    if (m_name != basename) {
        m_name = basename;
        emit nameChanged();
        updateNameKeyword();
    }
    updateFileNameKeyword();
}

void cwLazLayer::setPointSize(double pointSize)
{
    if (qFuzzyCompare(m_pointSize, pointSize)) {
        return;
    }
    m_pointSize = pointSize;
    emit pointSizeChanged();
}

void cwLazLayer::setEnabled(bool enabled)
{
    if (m_enabled == enabled) {
        return;
    }
    m_enabled = enabled;
    emit enabledChanged();

    if (!m_enabled) {
        // Cancel any in-flight build. asyncfuture's Restarter propagates the
        // outer cancel down to the inner worker, where cwPointOctreeBuilder
        // polls cancellation per chunk of points and per node, deletes its
        // temp directory, and writes no manifest. The observer chain below
        // still receives the canceled future; the m_enabled guard there drops
        // the result.
        m_loadRestarter.future().cancel();
        clearOctree();
        setErrorMessage(QString());
        // Only the points go: the file's CRS and bounding box are still known,
        // and a disabled layer that stopped being a georeferenced input would
        // read as one that was deleted.
        setPointState(PointState::Idle);
    } else {
        reload();
    }
}

QString cwLazLayer::sourceCS() const
{
    if (!m_sourceCSOverride.isEmpty()) {
        return m_sourceCSOverride;
    }
    return m_header.has_value() ? m_header->sourceCS : QString();
}

QString cwLazLayer::sourceCSDisplayName() const
{
    // PROJ resolves WKT, PROJ-strings, and authority codes uniformly, so the
    // same nameFor() call handles every flavor of sourceCS the loader can
    // produce. nameFor() has a thread_local cache keyed on the input string,
    // so binding re-evaluation in QML doesn't replay proj_create per row.
    return cwCoordinateTransform::nameFor(sourceCS());
}

void cwLazLayer::setSourceCSOverride(const QString& cs)
{
    if (m_sourceCSOverride == cs) {
        return;
    }

    // Which CRS the file is read as changes; what its header says does not, so
    // there is nothing to re-read. The precedence is applied in sourceCS().
    const QString previousCS = sourceCS();
    m_sourceCSOverride = cs;
    if (sourceCS() != previousCS) {
        emit sourceCSChanged();
        // Only an effective-CS move puts the points in the wrong place; an
        // override edit that resolves to the same CS changes nothing the
        // build reads.
        reload();
    }
}

cwGeoPoint cwLazLayer::sourceBboxMin() const
{
    return m_header.has_value() ? m_header->bboxMin : cwGeoPoint{};
}

cwGeoPoint cwLazLayer::sourceBboxMax() const
{
    return m_header.has_value() ? m_header->bboxMax : cwGeoPoint{};
}

cwGeoPoint cwLazLayer::sourceBboxCenter() const
{
    return midpoint(sourceBboxMin(), sourceBboxMax());
}

void cwLazLayer::setCacheRootPath(const QString& path)
{
    if (m_cacheRootPath == path) {
        return;
    }
    m_cacheRootPath = path;

    // The cache root is part of where the octree lives, so an octree published
    // under the old root no longer names this layer's octree — and a build
    // still running would write its nodes into the root the layer just left.
    if (!m_octree.isNull() || m_pointState == PointState::Loading) {
        reload();
    }
}

void cwLazLayer::setFutureManagerToken(const cwFutureManagerToken& token)
{
    m_futureManagerToken = token;
}

void cwLazLayer::setLocalProjectionToken(const cwLocalProjectionToken& token)
{
    m_localProjectionToken = token;
}

void cwLazLayer::reload()
{
    if (m_sourcePath.isEmpty()) {
        return;
    }

    if (!m_enabled) {
        // Disabled layers don't spend an async read. setSourcePath still
        // recorded path + fingerprint before calling us, so the layer stays
        // identifiable in the model.
        return;
    }

    setErrorMessage(QString());
    setPointState(PointState::Loading);

    // Points are only in the right place if the frame they were transformed
    // into is the one the project keeps, so the frame comes from the one place
    // that knows when it is final. On a project that already has a frame the
    // future is finished and the build starts here; on one still deriving its
    // frame from headers that are still arriving, waiting is what keeps every
    // cloud in the same frame — and keeps that frame from depending on which
    // header the disk returned first.
    ++m_reloadGeneration;
    const QFuture<QString> frame = m_localProjectionToken.frameFuture();
    if (frame.isFinished()) {
        startBuild(frame.result());
        return;
    }

    qCDebug(lcLazLayer) << QFileInfo(m_sourcePath).fileName()
                        << "reload() waiting on the frame";

    const quint64 generation = m_reloadGeneration;
    AsyncFuture::observe(frame).context(this,
        [this, generation](const QString& frameCS) {
            if (generation != m_reloadGeneration) {
                // A later reload() replaced this wait, and built against the
                // frame it was given.
                return;
            }
            if (!m_enabled || m_sourcePath.isEmpty()) {
                return;
            }
            startBuild(frameCS);
        },
        [this, generation]() {
            // The frame was abandoned with the project it belonged to. Give the
            // points up rather than report a load that will never run; the
            // header still stands, so the layer stays a georeferenced input.
            if (generation != m_reloadGeneration) {
                return;
            }
            setPointState(PointState::Idle);
        });
}

void cwLazLayer::startBuild(const QString& frameCS)
{
    qCDebug(lcLazLayer) << QFileInfo(m_sourcePath).fileName()
                        << "reload() frameCSSet=" << !frameCS.isEmpty();

    // A standalone layer has no project behind it, so its octree is cached
    // beside the file it was built from.
    const cwPointOctreeBuilder::Request request {
        .path = m_sourcePath,
        .sourceCSOverride = m_sourceCSOverride,
        .frameCS = frameCS,
        .cacheRootPath = m_cacheRootPath.isEmpty()
                ? QFileInfo(m_sourcePath).absolutePath()
                : m_cacheRootPath
    };
    m_buildCacheRootPath = request.cacheRootPath;
    const quint64 generation = m_reloadGeneration;

    // A build left over from an earlier reload() is now building against a
    // frame or a CS this layer has moved on from, and the probe below is
    // asynchronous, so it would have a whole probe's worth of time to publish
    // Loaded first. Cancelling here settles it: the Restarter pushes the outer
    // cancel down to the worker, and a canceled outer future never delivers.
    m_loadRestarter.future().cancel();

    // The probe reads one manifest and then stats one cache entry per node, so
    // a big octree is thousands of file hits — never on the GUI thread. It is
    // deliberately outside the restarter: a hit publishes the cached octree and
    // registers no job, which is what makes reopening a project instant.
    const QFuture<std::optional<cwPointOctreeManifest>> cached =
            cwConcurrent::run([request]() { return cwPointOctreeBuilder::cachedManifest(request); });

    AsyncFuture::observe(cached).context(this,
        [this, generation, request](std::optional<cwPointOctreeManifest> manifest) {
            if (generation != m_reloadGeneration) {
                // A later reload() replaced this probe and asked its own.
                return;
            }
            if (!m_enabled || m_sourcePath.isEmpty()) {
                return;
            }

            if (manifest.has_value()) {
                publishOctree(std::move(manifest.value()));
                return;
            }

            m_buildGeneration = generation;
            m_loadRestarter.restart([request]() {
                return cwPointOctreeBuilder::build(request);
            });
        });
}

void cwLazLayer::startHeaderProbe()
{
    if (m_sourcePath.isEmpty()) {
        return;
    }

    // Announced before the read is queued: the local projection waits for every
    // header it is going to be offered, and the wait has to be in place before
    // the reload() that follows this call asks whether the frame has settled.
    setHeaderProbeInFlight(true);

    // Runs whether or not the layer is enabled. Where the file sits is a fact
    // about the file, and the project's local projection is derived from it —
    // a layer that will never decode a point still has to be able to place one.
    const QString path = m_sourcePath;
    m_probeRestarter.restart([path]() {
        return cwConcurrent::run([path]() { return cwLazLoader::probeHeader(path); });
    });
}

void cwLazLayer::setHeaderProbeInFlight(bool inFlight)
{
    if (m_headerProbeInFlight == inFlight) {
        return;
    }
    m_headerProbeInFlight = inFlight;
    emit headerProbeInFlightChanged();
}

void cwLazLayer::finishProbe(const cwLazLoader::ProbeResult& probe)
{
    // An invalid probe means the file wouldn't open, and whatever a previous
    // probe established stands: rescan is what notices a file that has gone
    // away, and reload() is about to report the same failure as an Error.
    if (probe.valid) {
        const QString previousCS = sourceCS();
        const LoadStatus previousStatus = loadStatus();

        m_header = probe;

        if (sourceCS() != previousCS) {
            emit sourceCSChanged();
        }
        // Idle becomes Probed here; every other status already outranks it.
        if (loadStatus() != previousStatus) {
            emit loadStatusChanged();
        }
    }

    // Last on every path, so the header is published before the local
    // projection is told this layer has stopped being one it is waiting for.
    setHeaderProbeInFlight(false);
}

void cwLazLayer::applyResult(const cwPointOctreeBuilder::Result& result)
{
    if (result.hasError()) {
        clearOctree();
        setErrorMessage(result.errorMessage());
        setPointState(PointState::Error);
        return;
    }

    publishOctree(result.value());
}

void cwLazLayer::clearOctree()
{
    if (m_octree.isNull()) {
        return;
    }

    m_octree = cwPointOctreeSource{};
    m_pointCount = 0;
    m_bboxMin = QVector3D{};
    m_bboxMax = QVector3D{};
    m_meanSpacingXY = 0.0f;

    emit octreeChanged();
    emit pointCountChanged();
    emit bboxChanged();
    emit meanSpacingXYChanged();
}

void cwLazLayer::publishOctree(cwPointOctreeManifest&& manifest)
{
    m_pointCount = manifest.pointCount;
    m_bboxMin = manifest.bboxMin;
    m_bboxMax = manifest.bboxMax;
    m_meanSpacingXY = manifest.meanSpacingXY;

    const auto shared = std::make_shared<const cwPointOctreeManifest>(std::move(manifest));
    m_octree = cwPointOctreeSource(m_buildCacheRootPath,
                                   m_sourcePath,
                                   shared->fingerprint,
                                   shared);

    emit octreeChanged();
    emit pointCountChanged();
    emit bboxChanged();
    emit meanSpacingXYChanged();

    setErrorMessage(QString());
    // Last, so anything that reacts to Loaded reads the octree that is already
    // published rather than the one it replaced.
    setPointState(PointState::Loaded);
}

cwLazLayer::LoadStatus cwLazLayer::loadStatus() const
{
    switch (m_pointState) {
    case PointState::Idle:
        return m_header.has_value() ? LoadStatus::Probed : LoadStatus::Idle;
    case PointState::Loading:
        return LoadStatus::Loading;
    case PointState::Loaded:
        return LoadStatus::Loaded;
    case PointState::Error:
        return LoadStatus::Error;
    }
    return LoadStatus::Idle;
}

void cwLazLayer::setPointState(PointState state)
{
    if (m_pointState == state) {
        return;
    }
    m_pointState = state;
    emit loadStatusChanged();
}

void cwLazLayer::setErrorMessage(const QString& message)
{
    if (m_errorMessage == message) {
        return;
    }
    m_errorMessage = message;
    emit errorMessageChanged();
}

void cwLazLayer::updateNameKeyword()
{
    m_keywordModel->replace(cwKeyword(cwKeywordModel::NameKey, m_name));
}

void cwLazLayer::updateFileNameKeyword()
{
    m_keywordModel->replace(cwKeyword(cwKeywordModel::FileNameKey,
                                      QFileInfo(m_sourcePath).fileName()));
}

void cwLazLayer::updateIdKeyword()
{
    const QString shortId = m_id.toString(QUuid::WithoutBraces).left(8);
    m_keywordModel->replace(cwKeyword(cwKeywordModel::ObjectIdKey, shortId));
}

void cwLazLayer::updateTypeKeyword()
{
    m_keywordModel->replace(cwKeyword(cwKeywordModel::TypeKey,
                                      QString::fromLatin1(kLazLayerType)));
}
