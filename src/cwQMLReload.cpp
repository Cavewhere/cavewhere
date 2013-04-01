//Our includes
#include "cwQMLReload.h"
#include "cwDebug.h"

//Qt includes
#include <QQuickView>
#include <QQmlContext>
#include <QQmlEngine>
#include <QDebug>

cwQMLReload::cwQMLReload(QObject *parent) :
    QObject(parent),
    QuickView(NULL)
{
}

/**
Sets quickView
*/
void cwQMLReload::setQuickView(QQuickView* quickView) {
    if(QuickView != quickView) {
        QuickView = quickView;
        emit quickViewChanged();
    }
}

/**
 * @brief cwQMLReload::reload
 *
 * This will reload the qml, using a queue connection method invoke
 */
void cwQMLReload::reload()
{
    if(QuickView == NULL) {
        qDebug() << "Can't reload QML because the view is NULL, this is a BUG!" << LOCATION;
        return;
    }

    QUrl source = QuickView->source();

    QuickView->rootContext()->engine()->clearComponentCache();

    QMetaObject::invokeMethod(QuickView, "setSource", Qt::QueuedConnection, Q_ARG(QUrl, source));
}
