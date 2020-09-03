//Our includes
#include "cwPDFSettings.h"
#include "cwPDFConverter.h"

cwPDFSettings* cwPDFSettings::SettingsSingleton = nullptr;

cwPDFSettings::cwPDFSettings(QObject *parent) : QObject(parent)
{
    QSettings settings;
    int resolution = settings.value(importResolutionKey(), resolutionImport()).toInt();
    ResolutionImport = resolution;
}


bool cwPDFSettings::isSupportImport() const {
    return cwPDFConverter::isSupported();
}

void cwPDFSettings::setResolutionImport(int resolutionImport) {
    if(ResolutionImport != resolutionImport) {
        ResolutionImport = resolutionImport;
        QSettings settings;
        settings.setValue(importResolutionKey(), ResolutionImport);
        emit resolutionImportChanged();
    }
}

void cwPDFSettings::initialize()
{
    if(!SettingsSingleton) {
        Q_ASSERT(QThread::currentThread() == QCoreApplication::instance()->thread());
        SettingsSingleton = new cwPDFSettings(QCoreApplication::instance());
    }
}

cwPDFSettings *cwPDFSettings::instance()
{
    return SettingsSingleton;
}
