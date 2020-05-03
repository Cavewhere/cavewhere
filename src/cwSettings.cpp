//Our includes
#include "cwSettings.h"
#include "cwOpenGLSettings.h"
#include "cwJobSettings.h"

//Our inculdes
#include <QCoreApplication>
#include <QLocale>

cwSettings* cwSettings::Singleton = nullptr;

cwSettings::cwSettings(QObject *parent) : QObject(parent)
{
}

cwOpenGLSettings* cwSettings::renderingSettings() const {
    return cwOpenGLSettings::instance();
}

void cwSettings::initialize()
{
    if(Singleton == nullptr) {
        Singleton = new cwSettings(QCoreApplication::instance());
    }

    QLocale::setDefault(QLocale::c());

    //Keep these here because cwOpenGLSetting can be deallocated in cleanup()
    cwOpenGLSettings::initialize(); //Init's a singleton
    cwJobSettings::initialize();
}

cwSettings *cwSettings::instance()
{
    return Singleton;
}

cwJobSettings* cwSettings::jobSettings() const {
    return cwJobSettings::instance();
}
