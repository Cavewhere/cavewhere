//Our includes
#include "cwTrackedImage.h"
#include "cwImageDatabase.h"

//Std includes
#include <algorithm>

cwTrackedImage::cwTrackedImage() :
    Owner(NoOwnership)
{

}

cwTrackedImage::cwTrackedImage(const cwTrackedImage &image) :
    cwImage(image),
    Owner(image.Owner),
    Filename(image.Filename)
{

}

cwTrackedImage::cwTrackedImage(const cwImage &image, const QString &filename, int ownership) :
    cwImage(image),
    Owner(ownership),
    Filename(filename)
{
    // Q_ASSERT(QFile::exists(filename));
}

cwTrackedImage::cwTrackedImage(cwTrackedImage &&other) :
    cwImage(std::move(other))
{
    std::swap(Owner, other.Owner);
    std::swap(Filename, other.Filename);
}

cwTrackedImage &cwTrackedImage::operator=(const cwTrackedImage &other)
{
    if(this != &other) {
        Owner = other.Owner;
        Filename = other.Filename;
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

    //FIXME: Do we need to clean up passed tracked images for path images?

    // QList<int> imagesIds;
    // imagesIds.reserve(2 + mipmaps().size());
    // if(Owner & Original) {
    //     imagesIds.append(original());
    // }

    // if(Owner & Icon) {
    //     imagesIds.append(icon());
    // }

    // if(Owner & Mipmaps) {
    //     imagesIds.append(mipmaps());
    // }

    // QSet<int> uniqueImages;
    // for(auto id : imagesIds) {
    //     if(id > 0) {
    //         uniqueImages.insert(id);
    //     }
    // }

    // imagesIds = QList<int>(uniqueImages.begin(), uniqueImages.end());

    // if(!imagesIds.isEmpty()) {
    //     cwImageDatabase(Filename).removeImages(imagesIds);
    // }
}

void cwTrackedImage::sharedPtrDeleter(cwTrackedImage *image)
{
    image->deleteImagesFromDatabase();
    delete image;
}
