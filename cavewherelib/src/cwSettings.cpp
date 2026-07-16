//Our includes
#include "cwSettings.h"
#include "cwJobSettings.h"
#include "cwPDFSettings.h"
#include "cwFontSettings.h"
#include "cwSketchSettings.h"
#include "cwRenderingSettings.h"

//Qt includes
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
    cwFontSettings::initialize();
    cwSketchSettings::initialize();
    cwRenderingSettings::initialize();
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

cwFontSettings* cwSettings::fontSettings() const {
    return cwFontSettings::instance();
}

cwSketchSettings* cwSettings::sketchSettings() const {
    return cwSketchSettings::instance();
}

cwRenderingSettings* cwSettings::renderingSettings() const {
    return cwRenderingSettings::instance();
}
