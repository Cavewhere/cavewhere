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
#include "cwScrapViewMatrix.h"

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

    QList<cwTriangulateStation> stations() const;
    void setStations(QList<cwTriangulateStation> stations);

    cwNoteTranformation noteTransform() const;
    void setNoteTransform(cwNoteTranformation noteTransform);

    double noteImageResolution() const;
    void setNoteImageResolution(double dotsPerMeter);

    cwScrapViewMatrix viewMatrix() const;
    void setViewMatrix(const cwScrapViewMatrix& view);

    void setLookDirection(QVector3D eyeVector);
    QVector3D lookDirection() const;

    QList<cwLead> leads() const;
    void setLeads(QList<cwLead> leads);

private:
    QSharedDataPointer<cwTriangulatePrivateData> data;
};

#endif // CWTRIANGULATEINDATA_H
