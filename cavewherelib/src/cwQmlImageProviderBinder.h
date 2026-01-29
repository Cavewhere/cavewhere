/**************************************************************************
**
**    Copyright (C) 2026
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWQMLIMAGEPROVIDERBINDER_H
#define CWQMLIMAGEPROVIDERBINDER_H

#include <QObject>
#include <QPointer>
#include <QQmlEngine>

class cwCacheImageProvider;
class cwImageProvider;
class cwProject;
class cwRootData;

class cwQmlImageProviderBinder : public QObject
{
    Q_OBJECT

public:
    explicit cwQmlImageProviderBinder(QQmlEngine* engine, cwRootData* rootData, QObject* parent = nullptr);

    cwImageProvider* imageProvider() const { return m_imageProvider; }
    cwCacheImageProvider* cacheImageProvider() const { return m_cacheImageProvider; }

private:
    void ensureProviders();
    void bindProject();
    void clearProjectConnections();
    void updateFromProject();

    void setEngine(QQmlEngine* engine);
    void setRootData(cwRootData* rootData);

    QPointer<QQmlEngine> m_engine;
    QPointer<cwRootData> m_rootData;
    QPointer<cwProject> m_project;

    cwImageProvider* m_imageProvider = nullptr;
    cwCacheImageProvider* m_cacheImageProvider = nullptr;

    QMetaObject::Connection m_projectChangedConnection;
    QMetaObject::Connection m_filenameChangedConnection;
    QMetaObject::Connection m_dataRootChangedConnection;
};

#endif // CWQMLIMAGEPROVIDERBINDER_H
