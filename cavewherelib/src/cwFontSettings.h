#ifndef CWFONTSETTINGS_H
#define CWFONTSETTINGS_H

//Qt includes
#include <QObject>
#include <QString>
#include <QQmlEngine>

//Our includes
#include "cwGlobals.h"

class CAVEWHERE_LIB_EXPORT cwFontSettings : public QObject
{
    Q_OBJECT
    QML_NAMED_ELEMENT(FontSettings)
    QML_UNCREATABLE("FontSettings is a cavewhere singleton and can't be created directly")

    Q_PROPERTY(int fontBaseSize READ fontBaseSize WRITE setFontBaseSize NOTIFY fontBaseSizeChanged)
    Q_PROPERTY(int defaultFontBaseSize READ defaultFontBaseSize CONSTANT)
    Q_PROPERTY(QString fontFamily READ fontFamily WRITE setFontFamily NOTIFY fontFamilyChanged)

public:
    int fontBaseSize() const;
    void setFontBaseSize(int size);
    int defaultFontBaseSize() const;

    QString fontFamily() const;
    void setFontFamily(const QString& family);

    static cwFontSettings* instance();
    static void initialize();

signals:
    void fontBaseSizeChanged();
    void fontFamilyChanged();

private:
    explicit cwFontSettings(QObject* parent = nullptr);

    static cwFontSettings* Settings;

    static QString fontBaseSizeKey() { return QLatin1String("fontBaseSize"); }
    static QString fontFamilyKey()   { return QLatin1String("fontFamily"); }

    static constexpr int DefaultFontBaseSize = 16;
    int FontBaseSize = DefaultFontBaseSize;
    QString FontFamily = QLatin1String("Yanone Kaffeesatz");
};

inline int cwFontSettings::fontBaseSize() const { return FontBaseSize; }
inline int cwFontSettings::defaultFontBaseSize() const { return DefaultFontBaseSize; }
inline QString cwFontSettings::fontFamily() const { return FontFamily; }

#endif // CWFONTSETTINGS_H
