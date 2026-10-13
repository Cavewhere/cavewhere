//Our includes
#include "cwAppearanceSettings.h"

//Qt includes
#include <QCoreApplication>
#include <QGuiApplication>
#include <QSettings>
#include <QStyleHints>

namespace {
    QString colorSchemeKey() { return QStringLiteral("colorScheme"); }
}

cwAppearanceSettings* cwAppearanceSettings::Settings = nullptr;

cwAppearanceSettings::cwAppearanceSettings(QObject* parent) :
    QObject(parent)
{
    QSettings settings;
    const int stored = settings.value(colorSchemeKey(), System).toInt();
    m_colorScheme = (stored == Light || stored == Dark) ? static_cast<ColorScheme>(stored) : System;

    //System leaves the style hints as they are at startup: the QML test main
    //pins a light scheme for manual screenshots before this runs.
    if (m_colorScheme != System) {
        applyToStyleHints();
    }

    connect(QGuiApplication::styleHints(), &QStyleHints::colorSchemeChanged,
            this, &cwAppearanceSettings::updateDark);
    updateDark();
}

void cwAppearanceSettings::setColorScheme(ColorScheme colorScheme)
{
    if (m_colorScheme == colorScheme) {
        return;
    }
    m_colorScheme = colorScheme;

    QSettings settings;
    settings.setValue(colorSchemeKey(), static_cast<int>(colorScheme));

    emit colorSchemeChanged();
    applyToStyleHints();
    updateDark();
}

//Asks the platform to follow, so native title bars and dialogs match where the
//platform supports it. The hint is advisory: dark() below is computed from the
//setting itself and holds on every platform, including offscreen.
void cwAppearanceSettings::applyToStyleHints()
{
    QStyleHints* hints = QGuiApplication::styleHints();
    switch (m_colorScheme) {
    case System:
        hints->unsetColorScheme();
        break;
    case Light:
        hints->setColorScheme(Qt::ColorScheme::Light);
        break;
    case Dark:
        hints->setColorScheme(Qt::ColorScheme::Dark);
        break;
    }
}

void cwAppearanceSettings::updateDark()
{
    const bool systemDark = QGuiApplication::styleHints()->colorScheme() == Qt::ColorScheme::Dark;
    const bool dark = m_colorScheme == Dark || (m_colorScheme == System && systemDark);
    if (m_dark == dark) {
        return;
    }
    m_dark = dark;
    emit darkChanged();
}

cwAppearanceSettings* cwAppearanceSettings::instance()
{
    return Settings;
}

void cwAppearanceSettings::initialize()
{
    if (Settings == nullptr) {
        Settings = new cwAppearanceSettings(QCoreApplication::instance());
    }
}
