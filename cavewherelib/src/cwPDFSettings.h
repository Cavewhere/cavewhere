#ifndef CWPDFSETTINGS_H
#define CWPDFSETTINGS_H

//Qt includes
#include <QObject>
#include <QQmlEngine>

//Our includes
#include "cwGlobals.h"

class CAVEWHERE_LIB_EXPORT cwPDFSettings : public QObject
{
    Q_OBJECT
    QML_NAMED_ELEMENT(PDFSettings)
    QML_UNCREATABLE("PDFSettings is a cavewhere singleton and can't be created directly")

    Q_PROPERTY(bool isSupportImport READ isSupportImport CONSTANT)
    Q_PROPERTY(int resolutionImport READ resolutionImport WRITE setResolutionImport NOTIFY resolutionImportChanged)
    Q_PROPERTY(bool isAtDefaults READ isAtDefaults NOTIFY isAtDefaultsChanged)

public:
    static constexpr int DefaultResolutionImport = 96;

    bool isSupportImport() const;

    int resolutionImport() const;
    void setResolutionImport(int resolutionImport);

    bool isAtDefaults() const;
    Q_INVOKABLE void resetToDefaults();

    static void initialize();
    static cwPDFSettings* instance();

private:
    explicit cwPDFSettings(QObject *parent = nullptr);

    static cwPDFSettings* SettingsSingleton;

    int ResolutionImport = DefaultResolutionImport;

    static QString importResolutionKey() { return QLatin1String("importResolutionInPPI"); }

signals:
    void resolutionImportChanged();
    void isAtDefaultsChanged();

};

inline int cwPDFSettings::resolutionImport() const {
    return ResolutionImport;
}


#endif // CWPDFSETTINGS_H
