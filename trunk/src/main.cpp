//Qt includes
#include <QtGui/QApplication>
#include <QDeclarativeComponent>

//Our includes
#include "cwSurveyEditorMainWindow.h"
#include "cwStation.h"
#include "cwShot.h"
#include "cwSurveyChunk.h"
#include "cwCavingRegion.h"

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);

    qRegisterMetaType<QThread*>("QThread*");
    qRegisterMetaType<cwCavingRegion>("cwCavingRegion");
    qRegisterMetaType<QList <QString> >("QList<QString>");

    QApplication::setOrganizationName("Vadose Solutions");
    QApplication::setOrganizationDomain("cavewhere.com");
    QApplication::setApplicationName("Cavewhere");
    QApplication::setApplicationVersion("0.1");

    cwSurveyEditorMainWindow w;
    w.show();

    return a.exec();
}
