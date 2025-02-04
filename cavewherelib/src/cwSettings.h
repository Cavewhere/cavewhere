#ifndef CWSETTINGS_H
#define CWSETTINGS_H

//Qt includes
#include <QObject>
#include <QQmlEngine>

//Our includes
class cwJobSettings;
class cwPDFSettings;
#include "cwGlobals.h"

class CAVEWHERE_LIB_EXPORT cwSettings : public QObject
{
    Q_OBJECT
    QML_NAMED_ELEMENT(Settings)
    QML_UNCREATABLE("Settings is a cavewhere singleton and can't be created directly")

    Q_PROPERTY(cwJobSettings* jobSettings READ jobSettings CONSTANT)
    Q_PROPERTY(cwPDFSettings* pdfSettings READ pdfSettings CONSTANT)

public:
    cwJobSettings* jobSettings() const;
    cwPDFSettings* pdfSettings() const;

    static void initialize();
    static cwSettings* instance();

signals:

private:
    explicit cwSettings(QObject *parent = nullptr);

    static cwSettings* Singleton;

};

#endif // CWSETTINGS_H
