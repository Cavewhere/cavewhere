//Qt includes
#include <QtGui/QApplication>
#include <QDeclarativeComponent>

//Our includes
#include "cwSurveyEditorMainWindow.h"
#include "cwStation.h"
#include "cwShot.h"
#include "cwSurveyChunk.h"

int main(int argc, char *argv[])
{


    QApplication a(argc, argv);
    //a.setGraphicsSystem("opengl");

    QCoreApplication::setOrganizationName("Vadose Solutions");
    QCoreApplication::setOrganizationDomain("cavewhere.com");
    QApplication::setApplicationName("Cavewhere");
    QApplication::setApplicationVersion("0.1");



    cwSurveyEditorMainWindow w;
    w.show();

    return a.exec();
}
