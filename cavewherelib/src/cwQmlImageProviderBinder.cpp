/**************************************************************************
**
**    Copyright (C) 2026
**    www.cavewhere.com
**
**************************************************************************/

#include "cwQmlImageProviderBinder.h"

#include <QQmlEngine>

#include "cwCacheImageProvider.h"
#include "cwImageProvider.h"
#include "cwProject.h"
#include "cwRootData.h"

cwQmlImageProviderBinder::cwQmlImageProviderBinder(QQmlEngine* engine, cwRootData* rootData, QObject* parent)
    : QObject(parent)
{
    setEngine(engine);
    setRootData(rootData);
}

void cwQmlImageProviderBinder::setEngine(QQmlEngine* engine)
{
    if (m_engine == engine) {
        return;
    }

    m_engine = engine;
    ensureProviders();
    updateFromProject();
}

void cwQmlImageProviderBinder::setRootData(cwRootData* rootData)
{
    if (m_rootData == rootData) {
        return;
    }

    if (m_projectChangedConnection) {
        disconnect(m_projectChangedConnection);
        m_projectChangedConnection = {};
    }

    m_rootData = rootData;

    if (m_rootData) {
        m_projectChangedConnection = connect(m_rootData, &cwRootData::projectChanged, this, [this]() {
            bindProject();
        });
    }

    bindProject();
}

void cwQmlImageProviderBinder::ensureProviders()
{
    if (!m_engine) {
        return;
    }

    if (m_imageProvider == nullptr) {
        m_imageProvider = new cwImageProvider();
        m_engine->addImageProvider(cwImageProvider::name(), m_imageProvider);
    }

    if (m_cacheImageProvider == nullptr) {
        m_cacheImageProvider = new cwCacheImageProvider();
        m_engine->addImageProvider(cwCacheImageProvider::name(), m_cacheImageProvider);
    }
}

void cwQmlImageProviderBinder::clearProjectConnections()
{
    if (m_filenameChangedConnection) {
        disconnect(m_filenameChangedConnection);
        m_filenameChangedConnection = {};
    }
    if (m_dataRootChangedConnection) {
        disconnect(m_dataRootChangedConnection);
        m_dataRootChangedConnection = {};
    }
}

void cwQmlImageProviderBinder::bindProject()
{
    clearProjectConnections();

    if (!m_rootData) {
        m_project = nullptr;
        updateFromProject();
        return;
    }

    m_project = m_rootData->project();
    updateFromProject();

    if (m_project) {
        m_filenameChangedConnection = connect(m_project, &cwProject::filenameChanged, this, [this](const QString&) {
            updateFromProject();
        });
        m_dataRootChangedConnection = connect(m_project, &cwProject::dataRootChanged, this, [this]() {
            updateFromProject();
        });
    }
}

void cwQmlImageProviderBinder::updateFromProject()
{
    if (!m_project) {
        return;
    }

    ensureProviders();

    qDebug() << "project path:" << m_project->filename();
    qDebug() << "data root directory:" << m_project->dataRootDir();

    if (m_imageProvider) {
        m_imageProvider->setDataRootDir(m_project->dataRootDir());
    }

    if (m_cacheImageProvider) {
        m_cacheImageProvider->setProjectDirectory(m_project->dataRootDir());
    }
}
