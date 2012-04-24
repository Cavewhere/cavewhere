#ifndef CWSURVEXLRUDCHUNK_H
#define CWSURVEXLRUDCHUNK_H

//Qt includes
#include <QList>

//Our includes
#include "cwStation.h"
class cwSurvexImporter;

class cwSurvexLRUDChunk
{
    friend class cwSurvexImporter;

public:
    cwSurvexLRUDChunk();

private:
    QList<cwStation> Stations;
};

#endif // CWSURVEXLRUDCHUNK_H
