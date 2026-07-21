#ifndef CWSETTINGS_H
#define CWSETTINGS_H

//Qt includes
#include <QObject>
#include <QQmlEngine>

//Our includes
class cwJobSettings;
class cwPDFSettings;
class cwFontSettings;
class cwRenderingSettings;
#include "cwSketchSettings.h"
// Included (not forward-declared) so the unitSettings pointer property has a
// complete type in the concatenated moc TU, where moc_cwSettings precedes
// moc_cwUnitSettings alphabetically.
#include "cwUnitSettings.h"
#include "cwGlobals.h"

class CAVEWHERE_LIB_EXPORT cwSettings : public QObject
{
    Q_OBJECT
    QML_NAMED_ELEMENT(Settings)
    QML_UNCREATABLE("Settings is a cavewhere singleton and can't be created directly")

    Q_PROPERTY(cwJobSettings* jobSettings READ jobSettings CONSTANT)
    Q_PROPERTY(cwPDFSettings* pdfSettings READ pdfSettings CONSTANT)
    Q_PROPERTY(cwFontSettings* fontSettings READ fontSettings CONSTANT)
    Q_PROPERTY(cwSketchSettings* sketchSettings READ sketchSettings CONSTANT)
    Q_PROPERTY(cwRenderingSettings* renderingSettings READ renderingSettings CONSTANT)
    Q_PROPERTY(cwUnitSettings* unitSettings READ unitSettings CONSTANT)

public:
    cwJobSettings* jobSettings() const;
    cwPDFSettings* pdfSettings() const;
    cwFontSettings* fontSettings() const;
    cwSketchSettings* sketchSettings() const;
    cwRenderingSettings* renderingSettings() const;
    cwUnitSettings* unitSettings() const;

    static void initialize();
    static cwSettings* instance();

signals:

private:
    explicit cwSettings(QObject *parent = nullptr);

    static cwSettings* Singleton;

};

#endif // CWSETTINGS_H
