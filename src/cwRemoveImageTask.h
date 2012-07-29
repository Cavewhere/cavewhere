#ifndef CWREMOVEIMAGETASK_H
#define CWREMOVEIMAGETASK_H

//Our includes
#include "cwProjectIOTask.h"
#include "cwImage.h"

//Qt includes
#include <QList>

/**
 * @brief The cwRemoveImageTask class
*/
class cwRemoveImageTask : public cwProjectIOTask
{
    Q_OBJECT

public:
    cwRemoveImageTask(QObject* parent = NULL);

    void setImagesToRemove(QList<cwImage> images);

protected:
    virtual void runTask();

private:
    QList<cwImage> Images;

private slots:
    void tryToRemoveImages();

};

#endif // CWREMOVEIMAGETASK_H
