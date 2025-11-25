/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWTRIANGULATEINDATA_H
#define CWTRIANGULATEINDATA_H

//Qt includes
#include <QSharedData>
#include <QPolygonF>

//Our includes
#include "cwImage.h"
#include "cwTriangulateStation.h"
#include "cwNoteTranformation.h"
#include "cwLead.h"
#include "cwScrap.h"
#include "cwAbstractScrapViewMatrix.h"
#include "cwStationPositionLookup.h"
#include "cwSurveyNetwork.h"
#include "cwTriangulateWarpingData.h"

class cwTriangulatePrivateData;

class cwTriangulateInData
{
public:
    cwTriangulateInData();
    cwTriangulateInData(const cwTriangulateInData& data);
    cwTriangulateInData &operator=(const cwTriangulateInData& other);
    ~cwTriangulateInData();

    cwImage noteImage() const;
    void setNoteImage(cwImage noteImage);

    QPolygonF outline() const;
    void setOutline(QPolygonF outline);

    [[deprecated("Use note stations, network, and lookup instead")]]
    QList<cwTriangulateStation> stations() const;
    [[deprecated("Use note stations, network, and lookup instead")]]
    void setStations(QList<cwTriangulateStation> stations);

    QList<cwNoteStation> noteStations() const;
    void setNoteStation(const QList<cwNoteStation>& noteStations);

    cwStationPositionLookup stationLookup() const;
    void setStationLookup(const cwStationPositionLookup& lookup);

    cwSurveyNetwork surveyNetwork() const;
    void setSurveyNetwork(const cwSurveyNetwork& network);

    cwNoteTransformationData noteTransform() const;
    void setNoteTransform(const cwNoteTransformationData& noteTransform);

    double noteImageResolution() const;
    void setNoteImageResolution(double dotsPerMeter);

    cwAbstractScrapViewMatrix::Data* viewMatrix() const;
    void setViewMatrix(cwAbstractScrapViewMatrix::Data* view);

    void setLookDirection(QVector3D eyeVector);
    QVector3D lookDirection() const;

    QList<cwLead> leads() const;
    void setLeads(QList<cwLead> leads);

    cwTriangulateWarpingData morphingSettings() const;
    void setMorphingSettings(const cwTriangulateWarpingData& morphingSettings);

private:
    QSharedDataPointer<cwTriangulatePrivateData> data;
};

#endif // CWTRIANGULATEINDATA_H
