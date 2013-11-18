/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Our includes
#include "cwQMLReload.h"
#include "cwDebug.h"

//Qt includes
//#include <QQuickView>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQmlEngine>
#include <QDebug>

cwQMLReload::cwQMLReload(QObject *parent) :
    QObject(parent),
    ApplicationEngine(NULL)
{
}

/**
 * @brief cwQMLReload::reload
 *
 * This will reload the qml, using a queue connection method invoke
 */
void cwQMLReload::reload()
{
    if(ApplicationEngine == NULL) {
        qDebug() << "Can't reload QML because the ApplicationEngine is NULL, this is a BUG!" << LOCATION;
        return;
    }

    ApplicationEngine->clearComponentCache();

}

/**
Sets applicationEngine
*/
void cwQMLReload::setApplicationEngine(QQmlApplicationEngine* applicationEngine) {
    if(ApplicationEngine != applicationEngine) {
        ApplicationEngine = applicationEngine;
        emit applicationEngineChanged();
    }
}

/**
 * @brief cwQMLReload::setRootUrl
 * @param rootUrl - The root url, usually CavewhereMainWindow.qml
 */
void cwQMLReload::setRootUrl(QString rootUrl) {
    if(RootUrl != rootUrl) {
        RootUrl = rootUrl;
        emit rootUrlChanged();
    }
}
