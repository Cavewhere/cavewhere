//Qt includes
#include <QGuiApplication>
//#include <QQmlComponent>
#include <QThread>
#include <QtQuick/QQuickView>
#include <QQmlContext>
#include <QModelIndex>

//Our includes
//#include "cwMainWindow.h"
#include "cwStation.h"
#include "cwSurveyChunk.h"
#include "cwCavingRegion.h"
#include "cwImage.h"
#include "cwGlobalDirectory.h"
#include "cwStation.h"
#include "cwQMLRegister.h"
#include "cwRootData.h"


int main(int argc, char *argv[])
{
    QGuiApplication a(argc, argv);

    qRegisterMetaType<QThread*>("QThread*");
    qRegisterMetaType<cwCavingRegion>("cwCavingRegion");
    qRegisterMetaType<QList <QString> >("QList<QString>");
    qRegisterMetaType<QList <cwImage> >("QList<cwImage>");
    qRegisterMetaType<QList <cwStation > >("QList<cwStation>");
    qRegisterMetaType<QModelIndex>("QModelIndex");
    qRegisterMetaType<cwImage>("cwImage");

    QGuiApplication::setOrganizationName("Vadose Solutions");
    QGuiApplication::setOrganizationDomain("cavewhere.com");
    QGuiApplication::setApplicationName("Cavewhere");
    QGuiApplication::setApplicationVersion("0.1");

    cwGlobalDirectory::setupBaseDirectory();

    //Register all of the cavewhere types
    cwQMLRegister::registerQML();

    QQuickView view;

    cwRootData* rootData = new cwRootData(&view);
    view.rootContext()->setContextObject(rootData);
    view.rootContext()->setContextProperty("rootObject", (QObject*)view.rootObject());

    view.setResizeMode(QQuickView::SizeRootObjectToView);
    view.setSource(QUrl::fromLocalFile("qml/CavewhereMainWindow.qml"));
    view.show();

//    cwMainWindow w;
//    w.show();

    return a.exec();
}
