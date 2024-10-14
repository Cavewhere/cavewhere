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

    Q_PROPERTY(bool isSupportImport READ isSupportImport CONSTANT)
    Q_PROPERTY(int resolutionImport READ resolutionImport WRITE setResolutionImport NOTIFY resolutionImportChanged)

public:
    bool isSupportImport() const;

    int resolutionImport() const;
    void setResolutionImport(int resolutionImport);

    static void initialize();
    static cwPDFSettings* instance();

private:
    explicit cwPDFSettings(QObject *parent = nullptr);

    static cwPDFSettings* SettingsSingleton;

    int ResolutionImport = 300; //!<

    static QString importResolutionKey() { return QLatin1String("importResolutionInPPI"); }

signals:
    void resolutionImportChanged();

};

inline int cwPDFSettings::resolutionImport() const {
    return ResolutionImport;
}


#endif // CWPDFSETTINGS_H
