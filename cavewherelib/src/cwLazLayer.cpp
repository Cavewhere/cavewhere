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

    // Each restart() begins a new load run; register that run with the global
    // future manager and chain the result delivery onto it. Restarter cancels
    // the previous future and waits for it to settle before this fires again,
    // so we can never have two applyResult chains racing each other.
    // Register each load run with the global future manager. onResult() below
    // handles result delivery; the two compose (both fire when a fresh future
    // is installed).
    cwTrackRestarter(m_futureManagerToken, m_loadRestarter,
        [this]() { return QStringLiteral("Loading %1.laz").arg(m_name); });

    // Fires once per load run, only for a completed (non-cancelled, non-empty)
    // future. cwLazLoader always addResult()s except on cancellation — a header
    // open failure still yields an empty-geometry result — so applyResult()
    // owns all error reporting (its vertexCount() == 0 branch). The by-value
    // parameter is a shallow refcount bump: cwGeometry's vertex data is a CoW
    // QByteArray, not a deep copy.
    m_loadRestarter.onResult(this, [this](cwLazLoadResult result) {
        // The user may have disabled the layer between the load start and its
        // delivery. Drop the result — setEnabled(false) already cleared
        // geometry and reset status to Idle.
        if (!m_enabled) {
            return;
        }
        applyResult(std::move(result));
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
        // Cancel any in-flight load. asyncfuture's Restarter propagates the
        // outer cancel down to the inner worker, where cwLazLoader's
        // QPromise::isCanceled() check between point chunks short-circuits.
        // The observer chain below still receives the canceled future; the
        // m_enabled guard there drops the result.
        m_loadRestarter.future().cancel();
        const bool hadGeometry = (m_geometry.vertexCount() > 0);
        m_geometry = cwGeometry{};
        m_bboxMin = QVector3D{};
        m_bboxMax = QVector3D{};
        m_meanSpacingXY = 0.0f;
        setErrorMessage(QString());
        // Only the points go: the file's CRS and bounding box are still known,
        // and a disabled layer that stopped being a georeferenced input would
        // read as one that was deleted.
        setPointState(PointState::Idle);
        if (hadGeometry) {
            emit pointCountChanged();
            emit bboxChanged();
            emit meanSpacingXYChanged();
        }
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
        // Only an effective-CS move puts the decoded points in the wrong
        // place; an override edit that resolves to the same CS changes
        // nothing the decode reads.
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
    // future is finished and the decode starts here; on one still deriving its
    // frame from headers that are still arriving, waiting is what keeps every
    // cloud in the same frame — and keeps that frame from depending on which
    // header the disk returned first.
    ++m_frameWaitGeneration;
    const QFuture<QString> frame = m_localProjectionToken.frameFuture();
    if (frame.isFinished()) {
        startDecode(frame.result());
        return;
    }

    qCDebug(lcLazLayer) << QFileInfo(m_sourcePath).fileName()
                        << "reload() waiting on the frame";

    const quint64 generation = m_frameWaitGeneration;
    AsyncFuture::observe(frame).context(this,
        [this, generation](const QString& frameCS) {
            if (generation != m_frameWaitGeneration) {
                // A later reload() replaced this wait, and decoded against the
                // frame it was given.
                return;
            }
            if (!m_enabled || m_sourcePath.isEmpty()) {
                return;
            }
            startDecode(frameCS);
        },
        [this, generation]() {
            // The frame was abandoned with the project it belonged to. Give the
            // points up rather than report a load that will never run; the
            // header still stands, so the layer stays a georeferenced input.
            if (generation != m_frameWaitGeneration) {
                return;
            }
            setPointState(PointState::Idle);
        });
}

void cwLazLayer::startDecode(const QString& frameCS)
{
    qCDebug(lcLazLayer) << QFileInfo(m_sourcePath).fileName()
                        << "reload() frameCSSet=" << !frameCS.isEmpty();

    m_loadRestarter.restart([this, frameCS]() {
        return cwLazLoader::load({
            .path = m_sourcePath,
            .sourceCSOverride = m_sourceCSOverride,
            .frameCS = frameCS
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

void cwLazLayer::applyResult(cwLazLoadResult&& result)
{
    const QString previousCS = sourceCS();

    m_geometry = std::move(result.geometry);
    m_bboxMin = result.bboxMin;
    m_bboxMax = result.bboxMax;
    m_meanSpacingXY = result.meanSpacingXY;

    if (!m_header.has_value() && result.headerRead) {
        // A decode that read the header on its way lets a layer whose probe
        // hasn't landed yet still know where it is. Recorded as what the file
        // declares — not the override-resolved CS the decode used — matching
        // what the probe publishes when it arrives.
        m_header = cwLazLoader::ProbeResult{
            .valid = true,
            .sourceCS = result.embeddedCS,
            .bboxMin = result.sourceBboxMin,
            .bboxMax = result.sourceBboxMax
        };
    }

    if (sourceCS() != previousCS) {
        emit sourceCSChanged();
    }

    emit pointCountChanged();
    emit bboxChanged();
    emit meanSpacingXYChanged();

    if (m_geometry.vertexCount() == 0) {
        setErrorMessage(QStringLiteral("Could not read points from %1").arg(m_sourcePath));
        setPointState(PointState::Error);
        return;
    }

    setErrorMessage(QString());
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
