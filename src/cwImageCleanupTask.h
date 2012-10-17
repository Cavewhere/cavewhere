#ifndef CWIMAGECLEANUPTASK_H
#define CWIMAGECLEANUPTASK_H

//Our includes
#include "cwImage.h"
#include "cwProjectIOTask.h"
class cwCavingRegion;

//Qt includes
#include <QSet>

/**
 * @brief The cwImageCleanupTask class
 *
 * This removes un-used images from the database
 */
class cwImageCleanupTask : public cwProjectIOTask
{
public:
    cwImageCleanupTask();

    void setRegion(cwCavingRegion* region);
    cwCavingRegion* region() const;

protected:
    void runTask();

private:
    cwCavingRegion* Region;
    QList<cwImage> UnusedImages;
    QSet<int> DatabaseIds;

    void populateUnusedImages();
    QSet<int> extractAllValidImageIds();
    QSet<int> imageToSet(cwImage image) const;


private slots:
    void tryFetchAllImageIds();


};

/**
 * @brief cwImageCleanupTask::setRegion
 * @param region
 */
inline void cwImageCleanupTask::setRegion(cwCavingRegion *region)
{
    Region = region;
}

inline cwCavingRegion *cwImageCleanupTask::region() const
{
    return Region;
}


#endif // CWIMAGECLEANUPTASK_H
