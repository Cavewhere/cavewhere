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
    FontFamily = settings.value(fontFamilyKey(), FontFamily).toString();
    FontBaseSize = settings.value(fontBaseSizeKey(), defaultFontBaseSize()).toInt();
}

const QList<cwFontEntry>& cwFontSettings::fontEntries()
{
    static const QList<cwFontEntry> entries = {
        { QStringLiteral("CaveWhere"),  QStringLiteral("Yanone Kaffeesatz"),    16 },
        { QStringLiteral("Fira Sans"),  QStringLiteral("Fira Sans"),            14 },
        { QStringLiteral("System"),     QString(),                              14 },
    };
    return entries;
}

int cwFontSettings::defaultBaseSizeForFamily(const QString& family)
{
    for (const auto& entry : fontEntries()) {
        if (entry.family == family) {
            return entry.defaultSize;
        }
    }
    return 14;
}

void cwFontSettings::setFontBaseSize(int size)
{
    size = qBound(MinFontBaseSize, size, MaxFontBaseSize);
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
        int oldDefault = defaultBaseSizeForFamily(FontFamily);
        int offset = FontBaseSize - oldDefault;
        int oldSize = FontBaseSize;

        FontFamily = family;

        int newDefault = defaultBaseSizeForFamily(family);
        FontBaseSize = qBound(MinFontBaseSize, newDefault + offset, MaxFontBaseSize);

        QSettings settings;
        settings.setValue(fontFamilyKey(), family);
        settings.setValue(fontBaseSizeKey(), FontBaseSize);

        emit fontFamilyChanged();
        if (oldSize != FontBaseSize) {
            emit fontBaseSizeChanged();
        }
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
