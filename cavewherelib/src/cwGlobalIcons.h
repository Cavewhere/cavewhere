/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWGLOBALICONS_H
#define CWGLOBALICONS_H

//Qt includes
#include <QPixmapCache>
#include <QString>

/**
  This class hold static keys to global icons in cavewhere
  */
class cwGlobalIcons {
public:
    //Name
    static QPixmapCache::Key Cave;
    static QPixmapCache::Key NoImport;
    static QPixmapCache::Key Trip;
    static QPixmapCache::Key Plus;

    static QString caveFilename() { return QLatin1String(":/icons/cave.png");}
    static QString noImportFilename() { return QLatin1String(":/icons/dontImport.png");}
    static QString tripFilename() { return QLatin1String(":/icons/trip.png");}
    static QString plusFilename() { return QLatin1String(":/twbs-icons/icons/plus.svg");}

};

#endif // CWGLOBALICONS_H
