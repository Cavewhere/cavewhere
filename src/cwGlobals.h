/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWGLOBALS_H
#define CWGLOBALS_H

//Qt includes
#include <QString>
#include <QFileDialog>

/**
  These are required defines for exporting symbols in the cavewhere lib for
  windows. These do nothing on other platforms like mac and linux
  */
#if defined(CAVEWHERE_LIB)
#  define CAVEWHERE_LIB_EXPORT Q_DECL_EXPORT
#else
#  define CAVEWHERE_LIB_EXPORT Q_DECL_IMPORT
#endif


class cwGlobals
{
public:
    cwGlobals();

    static const double PI;
    static const double RadiansToDegrees;
    static const double DegreesToRadians;

    static QString addExtension(QString filename, QString extensionHint);
    static QString convertFromURL(QString filenameUrl);
    static QString findExecutable(QStringList executables);
};

#endif // CWGLOBALS_H
