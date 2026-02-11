#ifndef CWNOTEDATA_H
#define CWNOTEDATA_H

#include "cwImage.h"
#include "cwUnitValue.h"
#include "cwScrapData.h"
#include "cwImageResolution.h"
#include <QUuid>

struct cwNoteData {
    QString name;
    double rotate;
    cwImageResolution::Data imageResolution;
    QList<cwScrapData> scraps;
    cwImage image;
    QUuid id;
};

#endif // CWNOTEDATA_H
