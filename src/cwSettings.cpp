//
#include "cwSettings.h"
#include "cwOpenGLSettings.h"
#include "cwJobSettings.h"

//Our inculdes
#include <QCoreApplication>

cwSettings* cwSettings::Singleton = nullptr;

cwSettings::cwSettings(QObject *parent) : QObject(parent)
{
    cwOpenGLSettings::initialize(); //Init's a singleton
    cwJobSettings::initialize();
}

cwOpenGLSettings* cwSettings::renderingSettings() const {
    return cwOpenGLSettings::instance();
}

void cwSettings::initialize()
{
    if(Singleton == nullptr) {
        Singleton = new cwSettings(QCoreApplication::instance());
    }
}

cwSettings *cwSettings::instance()
{
    return Singleton;
}

cwJobSettings* cwSettings::jobSettings() const {
    return cwJobSettings::instance();
}
