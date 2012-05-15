//Qt includes
#include <QGuiApplication>
#include <QApplication>
//#include <QQmlComponent>
#include <QThread>
#include <QtQuick/QQuickView>
#include <QQmlContext>
#include <QModelIndex>
#include <QtWidgets/QWidget>
#include <QDir>

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
#include "cwProject.h"


int main(int argc, char *argv[])
{
    QApplication a(argc, argv);

    qRegisterMetaType<QThread*>("QThread*");
    qRegisterMetaType<cwCavingRegion>("cwCavingRegion");
    qRegisterMetaType<QList <QString> >("QList<QString>");
    qRegisterMetaType<QList <cwImage> >("QList<cwImage>");
    qRegisterMetaType<QList <cwStation > >("QList<cwStation>");
    qRegisterMetaType<QModelIndex>("QModelIndex");
    qRegisterMetaType<cwImage>("cwImage");

    QApplication::setOrganizationName("Vadose Solutions");
    QApplication::setOrganizationDomain("cavewhere.com");
    QApplication::setApplicationName("Cavewhere");
    QApplication::setApplicationVersion("0.1");

    cwGlobalDirectory::setupBaseDirectory();

    //Register all of the cavewhere types
    cwQMLRegister::registerQML();

    QQuickView view;

    QSurfaceFormat format = view.format();
    format.setSamples(2);

    cwRootData* rootData = new cwRootData(&view);
    rootData->project()->load(QDir::homePath() + "/Dropbox/quanko.cw");
    view.rootContext()->setContextObject(rootData);
    view.rootContext()->setContextProperty("mainWindow", &view);

    view.setFormat(format);
    view.setResizeMode(QQuickView::SizeRootObjectToView);
    view.setSource(QUrl::fromLocalFile("qml/CavewhereMainWindow.qml"));
    view.setWindowFlags(Qt::Window | Qt::WindowMinimizeButtonHint | Qt::WindowCloseButtonHint);
    view.show();

    a.setQuitOnLastWindowClosed(false);

    return a.exec();
}
