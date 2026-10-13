#ifndef CWAPPEARANCESETTINGS_H
#define CWAPPEARANCESETTINGS_H

//Qt includes
#include <QObject>
#include <QQmlEngine>

//Our includes
#include "cwGlobals.h"

/**
 * The Appearance choice of color scheme: follow the system, or force light or
 * dark. dark() is the answer the UI draws with, and Theme.qml reads it.
 */
class CAVEWHERE_LIB_EXPORT cwAppearanceSettings : public QObject
{
    Q_OBJECT
    QML_NAMED_ELEMENT(AppearanceSettings)
    QML_UNCREATABLE("AppearanceSettings is a cavewhere singleton and can't be created directly")

    Q_PROPERTY(ColorScheme colorScheme READ colorScheme WRITE setColorScheme NOTIFY colorSchemeChanged)
    Q_PROPERTY(bool dark READ dark NOTIFY darkChanged)

public:
    enum ColorScheme {
        System,
        Light,
        Dark
    };
    Q_ENUM(ColorScheme)

    ColorScheme colorScheme() const { return m_colorScheme; }
    void setColorScheme(ColorScheme colorScheme);

    bool dark() const { return m_dark; }

    static cwAppearanceSettings* instance();
    static void initialize();

signals:
    void colorSchemeChanged();
    void darkChanged();

private:
    explicit cwAppearanceSettings(QObject* parent = nullptr);

    void applyToStyleHints();
    void updateDark();

    static cwAppearanceSettings* Settings;

    ColorScheme m_colorScheme = System;
    bool m_dark = false;
};

#endif // CWAPPEARANCESETTINGS_H
