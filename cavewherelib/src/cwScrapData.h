#ifndef CWSCRAPDATA_H
#define CWSCRAPDATA_H

//Qt includes
#include <QPolygonF>

//Our includes
#include "cwLead.h"
#include "cwNoteStation.h"
#include "cwNoteTransformationData.h"
#include "cwAbstractScrapViewMatrix.h"

struct cwScrapData {

    cwScrapData() :
        viewMatrix(nullptr)
    {}

    cwScrapData(const QPolygonF& outlinePoints,
                const QList<cwNoteStation>& stations,
                const QList<cwLead>& leads,
                const cwNoteTransformationData& noteTransformation,
                bool calculateNoteTransform,
                std::unique_ptr<cwAbstractScrapViewMatrix::Data> viewMatrix) :
        outlinePoints(outlinePoints),
        stations(stations),
        leads(leads),
        noteTransformation(noteTransformation),
        calculateNoteTransform(calculateNoteTransform),
        viewMatrix(std::move(viewMatrix))
    {
    }

    //We need this because we need to clone the viewMatrix data
    cwScrapData(const cwScrapData& other) :
        outlinePoints(other.outlinePoints),
        stations(other.stations),
        leads(other.leads),
        noteTransformation(other.noteTransformation),
        calculateNoteTransform(other.calculateNoteTransform),
        viewMatrix(other.viewMatrix ? std::unique_ptr<cwAbstractScrapViewMatrix::Data>(other.viewMatrix->clone()) : nullptr)
    {

    }

    //We need this because we need to clone the viewMatrix data
    cwScrapData& operator=(const cwScrapData& other) {
        if (this != &other) {
            outlinePoints = other.outlinePoints;
            stations = other.stations;
            leads = other.leads;
            noteTransformation = other.noteTransformation;
            calculateNoteTransform = other.calculateNoteTransform;
            viewMatrix = other.viewMatrix ? std::unique_ptr<cwAbstractScrapViewMatrix::Data>(other.viewMatrix->clone()) : nullptr;
        }
        return *this;
    }

    QPolygonF outlinePoints;
    QList<cwNoteStation> stations;
    QList<cwLead> leads;
    cwNoteTransformationData noteTransformation;
    bool calculateNoteTransform;
    std::unique_ptr<cwAbstractScrapViewMatrix::Data> viewMatrix;
};


#endif // CWSCRAPDATA_H
