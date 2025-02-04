//Our includes
#include "cwSettings.h"
#include "cwJobSettings.h"
#include "cwPDFSettings.h"

//Our inculdes
#include <QCoreApplication>
#include <QLocale>

cwSettings* cwSettings::Singleton = nullptr;

cwSettings::cwSettings(QObject *parent) : QObject(parent)
{
}


void cwSettings::initialize()
{
    if(Singleton == nullptr) {
        Singleton = new cwSettings(QCoreApplication::instance());
    }

    QLocale::setDefault(QLocale::c());

    //Keep these here because cwOpenGLSetting can be deallocated in cleanup()
    cwJobSettings::initialize();
    cwPDFSettings::initialize();
}

cwSettings *cwSettings::instance()
{
    return Singleton;
}

cwJobSettings* cwSettings::jobSettings() const {
    return cwJobSettings::instance();
}

cwPDFSettings* cwSettings::pdfSettings() const {
    return cwPDFSettings::instance();
}
