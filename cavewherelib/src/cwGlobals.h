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
#include <QDir>

/**
  These are required defines for exporting symbols in the cavewhere lib for
  windows. These do nothing on other platforms like mac and linux
  */
#include "CaveWhereLibExport.h"

class CAVEWHERE_LIB_EXPORT cwGlobals
{
public:
    cwGlobals();

    static double pi();
    static double radiansToDegrees() { return 180.0 / cwGlobals::pi(); }
    static double degreesToRadians() { return cwGlobals::pi() / 180.0; }

    static QString addExtension(QString filename, QString extensionHint);
    static QString convertFromURL(QString filenameUrl);
    static QString findExecutable(QStringList executables);
    static QString findExecutable(const QStringList& executables, const QList<QDir>& dirs);
    static QList<QDir> systemPaths();
    static QList<QDir> survexPath();

    static void loadFonts();
};

namespace cw {

template<typename T, typename _Fn, typename R = std::invoke_result_t<_Fn&, T>>
static QList<R> transform(const QList<T>& list, _Fn func) {
    QList<R> returnList;
    returnList.reserve(list.size());
    std::transform(list.begin(), list.end(), std::back_inserter(returnList), func);
    return returnList;
};

template<typename T>
static QSet<T> toSet(const QList<T>& list) {
    return QSet<T>(list.begin(), list.end());
}

template<typename T>
static QList<T> toList(const QSet<T>& set) {
    return QList<T>(set.begin(), set.end());
}

}
#endif // CWGLOBALS_H
