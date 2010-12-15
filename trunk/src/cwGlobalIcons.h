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

    static const QString CaveFilename;
    static const QString NoImportFilename;
    static const QString TripFilename;

};

#endif // CWGLOBALICONS_H
