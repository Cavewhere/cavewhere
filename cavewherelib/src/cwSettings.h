#ifndef CWSETTINGS_H
#define CWSETTINGS_H

//Qt includes
#include <QObject>
#include <QQmlEngine>

//Our includes
class cwJobSettings;
class cwPDFSettings;
class cwFontSettings;
#include "cwGlobals.h"
// cwSketchSettings.h must be a full include (not a forward decl) because
// `cwSketchSettings` sorts after `cwSettings` in the auto-generated QML
// type registration file — forward-declaring here would leave the type
// incomplete at metatype-registration time and trip a static_assert.
#include "cwSketchSettings.h"

class CAVEWHERE_LIB_EXPORT cwSettings : public QObject
{
    Q_OBJECT
    QML_NAMED_ELEMENT(Settings)
    QML_UNCREATABLE("Settings is a cavewhere singleton and can't be created directly")

    Q_PROPERTY(cwJobSettings* jobSettings READ jobSettings CONSTANT)
    Q_PROPERTY(cwPDFSettings* pdfSettings READ pdfSettings CONSTANT)
    Q_PROPERTY(cwFontSettings* fontSettings READ fontSettings CONSTANT)
    Q_PROPERTY(cwSketchSettings* sketchSettings READ sketchSettings CONSTANT)

public:
    cwJobSettings* jobSettings() const;
    cwPDFSettings* pdfSettings() const;
    cwFontSettings* fontSettings() const;
    cwSketchSettings* sketchSettings() const;

    static void initialize();
    static cwSettings* instance();

signals:

private:
    explicit cwSettings(QObject *parent = nullptr);

    static cwSettings* Singleton;

};

#endif // CWSETTINGS_H
