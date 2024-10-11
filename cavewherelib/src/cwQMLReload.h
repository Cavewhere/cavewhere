/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWQMLRELOAD_H
#define CWQMLRELOAD_H

//Qt includes
#include <QObject>
#include <QQmlApplicationEngine>
class QQuickView;


//Our includes
#include "cwGlobals.h"

class CAVEWHERE_LIB_EXPORT cwQMLReload : public QObject
{
    Q_OBJECT

    Q_PROPERTY(QQmlApplicationEngine* applicationEngine READ applicationEngine WRITE setApplicationEngine NOTIFY applicationEngineChanged)
    Q_PROPERTY(QString rootUrl READ rootUrl WRITE setRootUrl NOTIFY rootUrlChanged)

public:
    explicit cwQMLReload(QObject *parent = 0);

    QQmlApplicationEngine* applicationEngine() const;
    void setApplicationEngine(QQmlApplicationEngine* applicationEngine);

    QString rootUrl() const;
    void setRootUrl(QString rootUrl);

    Q_INVOKABLE void reload();

signals:
    void applicationEngineChanged();
    void rootUrlChanged();

public slots:

private:
    QQmlApplicationEngine* ApplicationEngine; //!<
    QString RootUrl; //!<
    
};

///**
//Gets quickView
//*/
//inline QQuickView* cwQMLReload::quickView() const {
//    return QuickView;
//}

/**
Gets applicationEngine
*/
inline QQmlApplicationEngine* cwQMLReload::applicationEngine() const {
    return ApplicationEngine;
}

/**
Gets rootUrl
*/
inline QString cwQMLReload::rootUrl() const {
    return RootUrl;
}


#endif // CWQMLRELOAD_H
