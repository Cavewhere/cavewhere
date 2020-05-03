#ifndef CWIMAGEDATABASE_H
#define CWIMAGEDATABASE_H

//Qt includes
#include <QSqlDatabase>

//Our includes
#include "cwImageData.h"
#include "cwImage.h"
#include "cwGlobals.h"

class CAVEWHERE_LIB_EXPORT cwImageDatabase
{
public:
    cwImageDatabase(const QString& filename = QString());
    cwImageDatabase(const cwImageDatabase&) = delete;
    ~cwImageDatabase();

    QString filename() const;
    void setFilename(const QString& filename);

    int addImage(const cwImageData& imageData);
    bool updateImage(const cwImageData& imageData, int id);
    int addOrUpdateImage(const cwImageData& imageData, int id);

    bool removeImage(cwImage image, bool withTransaction = true);
    bool removeImages(QList<int> ids, bool withTransaction = true);

    bool imageExists(int id) const;
    bool mipmapsValid(cwImage image, bool usingCompression) const;

private:
    QSqlDatabase Database;
};

inline QString cwImageDatabase::filename() const
{
    return Database.databaseName();
}


#endif // CWIMAGEDATABASE_H
