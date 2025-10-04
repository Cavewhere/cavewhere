#ifndef CWTRIANGULATELIDARTASK_H
#define CWTRIANGULATELIDARTASK_H

//Our includes
#include "cwTriangulateLiDARInData.h"

//Qt includes
#include <QFuture>

class cwTriangulateLiDARTask
{
public:
    cwTriangulateLiDARTask() = delete;

    static QFuture<void> triangulate(QList<cwTriangulateLiDARInData> liDARs);
};

#endif // CWTRIANGULATELIDARTASK_H
