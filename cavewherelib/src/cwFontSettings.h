#ifndef CWFONTSETTINGS_H
#define CWFONTSETTINGS_H

//Qt includes
#include <QObject>
#include <QString>
#include <QQmlEngine>

//Our includes
#include "cwGlobals.h"

struct CAVEWHERE_LIB_EXPORT cwFontEntry
{
    Q_GADGET
    Q_PROPERTY(QString label MEMBER label)
    Q_PROPERTY(QString family MEMBER family)
    Q_PROPERTY(int defaultSize MEMBER defaultSize)
public:
    QString label;
    QString family;
    int defaultSize;
};

class CAVEWHERE_LIB_EXPORT cwFontSettings : public QObject
{
    Q_OBJECT
    QML_NAMED_ELEMENT(FontSettings)
    QML_UNCREATABLE("FontSettings is a cavewhere singleton and can't be created directly")

    Q_PROPERTY(int fontBaseSize READ fontBaseSize WRITE setFontBaseSize NOTIFY fontBaseSizeChanged)
    Q_PROPERTY(int defaultFontBaseSize READ defaultFontBaseSize NOTIFY fontFamilyChanged)
    Q_PROPERTY(QString fontFamily READ fontFamily WRITE setFontFamily NOTIFY fontFamilyChanged)
    Q_PROPERTY(QList<cwFontEntry> fontEntries READ fontEntries CONSTANT)
    Q_PROPERTY(int minFontBaseSize READ minFontBaseSize CONSTANT)
    Q_PROPERTY(int maxFontBaseSize READ maxFontBaseSize CONSTANT)
    Q_PROPERTY(bool isAtDefaults READ isAtDefaults NOTIFY isAtDefaultsChanged)

public:
    static constexpr int MinFontBaseSize = 10;
    static constexpr int MaxFontBaseSize = 28;

    int minFontBaseSize() const { return MinFontBaseSize; }
    int maxFontBaseSize() const { return MaxFontBaseSize; }

    int fontBaseSize() const;
    void setFontBaseSize(int size);
    int defaultFontBaseSize() const;
    static int defaultBaseSizeForFamily(const QString& family);
    static const QList<cwFontEntry>& fontEntries();

    QString fontFamily() const;
    void setFontFamily(const QString& family);

    bool isAtDefaults() const;
    Q_INVOKABLE void resetToDefaults();

    static cwFontSettings* instance();
    static void initialize();

signals:
    void fontBaseSizeChanged();
    void fontFamilyChanged();
    void isAtDefaultsChanged();

private:
    explicit cwFontSettings(QObject* parent = nullptr);

    static cwFontSettings* Settings;

    static QString fontBaseSizeKey() { return QLatin1String("fontBaseSize"); }
    static QString fontFamilyKey()   { return QLatin1String("fontFamily"); }

    int FontBaseSize = fontEntries().first().defaultSize;
    QString FontFamily = fontEntries().first().family;
};

inline int cwFontSettings::fontBaseSize() const { return FontBaseSize; }
inline int cwFontSettings::defaultFontBaseSize() const { return defaultBaseSizeForFamily(FontFamily); }
inline QString cwFontSettings::fontFamily() const { return FontFamily; }

#endif // CWFONTSETTINGS_H
