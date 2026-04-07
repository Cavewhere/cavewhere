//Our includes
#include "cwFontSettings.h"

//Qt includes
#include <QSettings>
#include <QCoreApplication>

cwFontSettings* cwFontSettings::Settings = nullptr;

cwFontSettings::cwFontSettings(QObject* parent) :
    QObject(parent)
{
    QSettings settings;
    FontBaseSize = settings.value(fontBaseSizeKey(), DefaultFontBaseSize).toInt();
    FontFamily = settings.value(fontFamilyKey(), FontFamily).toString();
}

void cwFontSettings::setFontBaseSize(int size)
{
    if (FontBaseSize != size) {
        FontBaseSize = size;
        QSettings settings;
        settings.setValue(fontBaseSizeKey(), size);
        emit fontBaseSizeChanged();
    }
}

void cwFontSettings::setFontFamily(const QString& family)
{
    if (FontFamily != family) {
        FontFamily = family;
        QSettings settings;
        settings.setValue(fontFamilyKey(), family);
        emit fontFamilyChanged();
    }
}

cwFontSettings* cwFontSettings::instance()
{
    return Settings;
}

void cwFontSettings::initialize()
{
    if (Settings == nullptr) {
        Settings = new cwFontSettings(QCoreApplication::instance());
    }
}
