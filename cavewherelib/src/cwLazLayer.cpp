/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#include "cwLazLayer.h"

//Qt includes
#include <QDebug>
#include <QFileInfo>
#include <QLoggingCategory>

//Our includes
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
    m_loadRestarter.onFutureChanged([this]() {
        if (m_futureManagerToken.isValid()) {
            m_futureManagerToken.addJob(cwFuture(
                QFuture<void>(m_loadRestarter.future()),
                QStringLiteral("Loading %1.laz").arg(m_name)));
        }

        AsyncFuture::observe(m_loadRestarter.future())
            .context(this, [this]() {
                QFuture<cwLazLoadResult> finished = m_loadRestarter.future();
                if (finished.resultCount() == 0) {
                    setErrorMessage(QStringLiteral("Load failed: %1").arg(m_sourcePath));
                    setLoadStatus(LoadStatus::Error);
                    return;
                }
                // result() returns const T& — the local copy is a shallow
                // refcount bump (cwGeometry's vertex data is QByteArray,
                // CoW), so this isn't a deep copy. takeResult() returns a
                // default-constructed value when delivered through an
                // AsyncFuture Deferred — copy is the safe path.
                cwLazLoadResult result = finished.result();
                applyResult(std::move(result));
            });
    });
}

cwLazLayer::~cwLazLayer() = default;

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

void cwLazLayer::setPointSize(double pointSize)
{
    if (qFuzzyCompare(m_pointSize, pointSize)) {
        return;
    }
    m_pointSize = pointSize;
    emit pointSizeChanged();
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
