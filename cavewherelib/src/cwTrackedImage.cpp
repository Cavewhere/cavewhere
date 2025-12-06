//Our includes
#include "cwTrackedImage.h"

//Std includes
#include <algorithm>

cwTrackedImage::cwTrackedImage() :
    Owner(NoOwnership)
{

}

cwTrackedImage::cwTrackedImage(const cwTrackedImage &image) :
    cwImage(image),
    Owner(image.Owner)
{

}

cwTrackedImage::cwTrackedImage(const cwImage &image, const QString &filename, int ownership) :
    cwImage(image),
    Owner(ownership)
{
    setPath(filename);
    // Q_ASSERT(QFile::exists(filename));
}

cwTrackedImage::cwTrackedImage(cwTrackedImage &&other) :
    cwImage(std::move(other))
{
    std::swap(Owner, other.Owner);
}

cwTrackedImage &cwTrackedImage::operator=(const cwTrackedImage &other)
{
    if(this != &other) {
        Owner = other.Owner;
        cwImage::operator=(other);
    }
    return *this;
}

bool cwTrackedImage::operator==(const cwTrackedImage &other) const
{
    return cwImage::operator==(other);
}

bool cwTrackedImage::operator!=(const cwTrackedImage &other) const
{
    return !operator==(other);
}

cwImage cwTrackedImage::take()
{
    Owner = NoOwnership;
    return std::move(*this);
}

QSharedPointer<cwTrackedImage> cwTrackedImage::createShared(const cwImage &image, const QString &filename, int ownership)
{
    return QSharedPointer<cwTrackedImage>(new cwTrackedImage(image, filename, ownership), sharedPtrDeleter);
}

QSharedPointer<cwTrackedImage> cwTrackedImage::createShared(int original, const QString &filename, int ownership)
{
    cwImage image;
    image.setOriginal(original);
    return QSharedPointer<cwTrackedImage>(new cwTrackedImage(image, filename, ownership), sharedPtrDeleter);
}

void cwTrackedImage::deleteImagesFromDatabase() const
{
    if(Owner == NoOwnership) {
        return;
    }

    Q_ASSERT(QFile::exists(path()));
    if(QFile::exists(path())) {
        QFile::remove(path());
    }
}

void cwTrackedImage::sharedPtrDeleter(cwTrackedImage *image)
{
    image->deleteImagesFromDatabase();
    delete image;
}
