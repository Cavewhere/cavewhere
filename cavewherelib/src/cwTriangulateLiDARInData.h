/**************************************************************************
**
**    Copyright (C) 2025 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWTRIANGULATELIDARINDATA_H
#define CWTRIANGULATELIDARINDATA_H

// Qt includes
#include <QSharedData>
#include <QString>
#include <QList>

// Our includes
#include "cwNoteLiDARStation.h"
#include "cwStationPositionLookup.h"
#include "cwSurveyNetwork.h"
#include "CaveWhereLibExport.h"

/**
 * @brief The cwTriangulateLiDARInData class
 *
 * Encapsulates input data required for triangulating LiDAR note data.
 * This includes LiDAR station information, the survey network,
 * station position lookup, and a GLTF filename for reference geometry.
 */
class cwTriangulateLiDARPrivateData;

class CAVEWHERE_LIB_EXPORT cwTriangulateLiDARInData
{
public:
    cwTriangulateLiDARInData();
    cwTriangulateLiDARInData(const cwTriangulateLiDARInData& other);
    cwTriangulateLiDARInData& operator=(const cwTriangulateLiDARInData& other);
    ~cwTriangulateLiDARInData();

    QList<cwNoteLiDARStation> noteStations() const;
    void setNoteStations(const QList<cwNoteLiDARStation>& stations);

    cwStationPositionLookup stationLookup() const;
    void setStationLookup(const cwStationPositionLookup& lookup);

    cwSurveyNetwork surveyNetwork() const;
    void setSurveyNetwork(const cwSurveyNetwork& network);

    QString gltfFilename() const;
    void setGltfFilename(const QString& filename);

    QMatrix4x4 modelMatrix() const;
    void setModelMatrix(const QMatrix4x4& matrix);

private:
    QSharedDataPointer<cwTriangulateLiDARPrivateData> data;
};

#endif // CWTRIANGULATELIDARINDATA_H
