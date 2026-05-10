/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#include "cwLazLayer.h"

//Qt includes
#include <QFileInfo>
#include <QFutureWatcher>

//AsyncFuture
#include <asyncfuture.h>

//Our includes
#include "cwFuture.h"
#include "cwKeyword.h"
#include "cwKeywordModel.h"

namespace {
constexpr const char* kLazLayerType = "LAZ Layer";
}

cwLazLayer::cwLazLayer(QObject* parent) :
    QObject(parent),
    m_id(QUuid::createUuid()),
    m_keywordModel(new cwKeywordModel(this))
{
    updateTypeKeyword();
    updateIdKeyword();
}

cwLazLayer::~cwLazLayer()
{
    // Per CLAUDE.md: cancel in-flight futures so the worker stops reading
    // when the layer is destroyed.
    m_loadFuture.cancel();
}

void cwLazLayer::setSourcePath(const QString& path)
{
    if (m_sourcePath == path) {
        return;
    }
    m_sourcePath = path;
    emit sourcePathChanged();

    const QString basename = QFileInfo(path).baseName();
    if (m_name != basename) {
        m_name = basename;
        emit nameChanged();
        updateNameKeyword();
    }
    updateFileNameKeyword();

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

    // Cancel any prior in-flight load so the supersede path is explicit, not
    // just an epoch check that lets the old worker run to completion.
    m_loadFuture.cancel();

    setErrorMessage(QString());
    setLoadStatus(LoadStatus::Loading);
    m_loadProgress = 0.0;
    emit loadProgressChanged();

    const quint64 epoch = ++m_loadEpoch;

    m_loadFuture = cwLazLoader::load({
        .path = m_sourcePath,
        .sourceCSOverride = m_sourceCSOverride,
        .globalCS = m_regionGlobalCS,
        .worldOrigin = m_regionWorldOrigin
    });

    if (m_futureManagerToken.isValid()) {
        m_futureManagerToken.addJob(cwFuture(
            QFuture<void>(m_loadFuture),
            QStringLiteral("Loading %1.laz").arg(m_name)));
    }

    auto* watcher = new QFutureWatcher<cwLazLoadResult>(this);
    connect(watcher, &QFutureWatcher<cwLazLoadResult>::progressValueChanged,
            this, [this, epoch, watcher]() {
        if (epoch != m_loadEpoch) return;
        const int min = watcher->progressMinimum();
        const int max = watcher->progressMaximum();
        const int v = watcher->progressValue();
        const double frac = (max > min) ? double(v - min) / double(max - min) : 0.0;
        if (!qFuzzyCompare(m_loadProgress, frac)) {
            m_loadProgress = frac;
            emit loadProgressChanged();
        }
    });
    connect(watcher, &QFutureWatcher<cwLazLoadResult>::finished,
            this, [this, epoch, watcher]() {
        watcher->deleteLater();
        if (epoch != m_loadEpoch) {
            return;
        }
        if (watcher->isCanceled() || watcher->future().resultCount() == 0) {
            setErrorMessage(QStringLiteral("Load cancelled or failed: %1").arg(m_sourcePath));
            setLoadStatus(LoadStatus::Error);
            return;
        }
        cwLazLoadResult result = watcher->result();
        applyResult(std::move(result));
    });
    watcher->setFuture(m_loadFuture);
}

void cwLazLayer::applyResult(cwLazLoadResult&& result)
{
    m_geometry = std::move(result.geometry);
    m_bboxMin = result.bboxMin;
    m_bboxMax = result.bboxMax;

    if (m_sourceCS != result.sourceCS) {
        m_sourceCS = result.sourceCS;
        emit sourceCSChanged();
    }

    emit pointCountChanged();
    emit bboxChanged();

    m_loadProgress = 1.0;
    emit loadProgressChanged();

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
