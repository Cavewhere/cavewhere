#ifndef CWTRACKEDIMAGE_H
#define CWTRACKEDIMAGE_H

//Our includes
#include "cwImage.h"
#include "cwGlobals.h"

//Qt includes
#include <QSharedPointer>

class CAVEWHERE_LIB_EXPORT cwTrackedImage : public cwImage
{
public:
    enum OwnerShip {
        NoOwnership = 0x0,
        Original = 1 << 0,
        Icon = 1 << 1,
        Mipmaps = 1 << 2
    };

    cwTrackedImage();
    cwTrackedImage(const cwTrackedImage& image);
    cwTrackedImage(const cwImage &image,
                   const QString &filename,
                   int ownership = (Original | Icon | Mipmaps));
    cwTrackedImage(cwTrackedImage&& other);
    ~cwTrackedImage() = default;

    cwTrackedImage& operator=(const cwTrackedImage& other);

    bool operator==(const cwTrackedImage& other) const;
    bool operator!=(const cwTrackedImage& other) const;

    cwImage take();

    static QSharedPointer<cwTrackedImage> createShared(const cwImage &image,
                                                       const QString &filename,
                                                       int ownership = (Original | Icon | Mipmaps));
    static QSharedPointer<cwTrackedImage> createShared(int original,
                                                       const QString &filename,
                                                       int ownership = (Original | Icon | Mipmaps));

    void deleteImagesFromDatabase() const;
    static void sharedPtrDeleter(cwTrackedImage* image);

private:
    int Owner;
    QString Filename;

};

typedef QSharedPointer<cwTrackedImage> cwTrackedImagePtr;

#endif // CWTRACKEDIMAGE_H
