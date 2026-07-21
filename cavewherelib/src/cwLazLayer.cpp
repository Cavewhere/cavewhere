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
#include "cwCoordinateTransform.h"
#include "cwFuture.h"
#include "cwKeyword.h"
#include "cwKeywordModel.h"

Q_LOGGING_CATEGORY(lcLazLayer, "cw.laz.layer")

namespace {
constexpr const char* kLazLayerType = "LAZ Layer";
}

cwLazLayer::cwLazLayer(QObject* parent) :
    QObject(parent),
    m_id(QUuid::createUuid()),
    m_keywordModel(new cwKeywordModel(this)),
    m_loadRestarter(this)
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
        setLoadStatus(LoadStatus::Idle);
        if (hadGeometry) {
            emit pointCountChanged();
            emit bboxChanged();
            emit meanSpacingXYChanged();
        }
    } else {
        reload();
    }
}

QString cwLazLayer::sourceCSDisplayName() const
{
    // PROJ resolves WKT, PROJ-strings, and authority codes uniformly, so the
    // same nameFor() call handles every flavor of sourceCS the loader can
    // produce. nameFor() has a thread_local cache keyed on the input string,
    // so binding re-evaluation in QML doesn't replay proj_create per row.
    return cwCoordinateTransform::nameFor(m_sourceCS);
}

void cwLazLayer::setSourceCSOverride(const QString& cs)
{
    if (m_sourceCSOverride == cs) {
        return;
    }
    m_sourceCSOverride = cs;
    reload();
}

void cwLazLayer::setFutureManagerToken(const cwFutureManagerToken& token)
{
    m_futureManagerToken = token;
}

void cwLazLayer::setRegionGlobalCS(const QString& cs)
{
    if (m_regionGlobalCS == cs) {
        return;
    }
    m_regionGlobalCS = cs;
    if (!m_sourcePath.isEmpty()) {
        reload();
    }
}

void cwLazLayer::setRegionWorldOrigin(const cwGeoPoint& origin)
{
    if (m_regionWorldOrigin == origin) {
        return;
    }
    m_regionWorldOrigin = origin;
    if (!m_sourcePath.isEmpty()) {
        reload();
    }
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

    qCDebug(lcLazLayer) << QFileInfo(m_sourcePath).fileName()
                        << "reload() worldOrigin=("
                        << m_regionWorldOrigin.x << ","
                        << m_regionWorldOrigin.y << ","
                        << m_regionWorldOrigin.z << ")"
                        << "globalCSSet=" << !m_regionGlobalCS.isEmpty();

    setErrorMessage(QString());
    setLoadStatus(LoadStatus::Loading);

    m_loadRestarter.restart([this]() {
        return cwLazLoader::load({
            .path = m_sourcePath,
            .sourceCSOverride = m_sourceCSOverride,
            .globalCS = m_regionGlobalCS,
            .worldOrigin = m_regionWorldOrigin
        });
    });
}

void cwLazLayer::applyResult(cwLazLoadResult&& result)
{
    m_geometry = std::move(result.geometry);
    m_bboxMin = result.bboxMin;
    m_bboxMax = result.bboxMax;
    m_meanSpacingXY = result.meanSpacingXY;

    if (m_sourceCS != result.sourceCS) {
        m_sourceCS = result.sourceCS;
        emit sourceCSChanged();
    }

    emit pointCountChanged();
    emit bboxChanged();
    emit meanSpacingXYChanged();

    if (m_geometry.vertexCount() == 0) {
        setErrorMessage(QStringLiteral("Could not read points from %1").arg(m_sourcePath));
        setLoadStatus(LoadStatus::Error);
        return;
    }

    setErrorMessage(QString());
    setLoadStatus(LoadStatus::Loaded);
}

void cwLazLayer::setLoadStatus(LoadStatus status)
{
    if (m_loadStatus == status) {
        return;
    }
    m_loadStatus = status;
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
