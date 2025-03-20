/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Our includes
#include "cwBaseScrapInteraction.h"
#include "cwDebug.h"
#include "cwScrap.h"
#include "cwNote.h"
// #include "cwImageItem.h"
#include "cwScrapOutlinePointView.h"
#include "cwTrip.h"
#include "cwSurveyNoteModel.h"
#include "cwScrapView.h"
// #include "cwNoteCamera.h"

cwBaseScrapInteraction::cwBaseScrapInteraction(QQuickItem *parent) :
    cwNoteInteraction(parent),
    m_scrap(nullptr),
    m_outlinePointView(nullptr)
{
    connect(this, SIGNAL(noteChanged()), SLOT(startNewScrap()));
    connect(this, SIGNAL(visibleChanged()), SLOT(deactivating()));
}

/**
    This adds a new scrap to the note and also make the current scrap equal to the newly add
    scrap
  */
void cwBaseScrapInteraction::addScrap() {
    //Get the last transform
    cwNoteTranformation* transform = lastNoteTransformation();

    setScrap(new cwScrap());

    if(transform != nullptr) {
        //Used the previous note transform
        scrap()->setCalculateNoteTransform(false);
        *(scrap()->noteTransformation()) = *transform; //Copy the note transform data to the scrap
    }

    note()->addScrap(scrap());
}

/**
 * @brief cwBaseScrapInteraction::closeCurrentScrap
 *
 * Closes the current scrap geometry
 */
void cwBaseScrapInteraction::closeCurrentScrap() {
    if(m_scrap != nullptr) {
        m_scrap->close();
    }
}

cwBaseScrapInteraction::cwClosestPoint cwBaseScrapInteraction::calcClosestPoint(QPoint qtViewportPosition) const
{
    if(note() == nullptr) {
        return cwClosestPoint();
    }

    if(m_scrap == nullptr) {
        return cwClosestPoint();
    }

    auto toNote = cwScrapView::toNormalized(note());
    QPointF noteCoordinate = toNote.map(qtViewportPosition.toPointF());
    double buffer = 7.5; //In pixels

    //Go through each scrap polygon's lines
    for(int i = 0; i < m_scrap->numberOfPoints() - 1; i++) {
        QPointF p1 = m_scrap->points().at(i);
        QPointF p2 = m_scrap->points().at(i+1);
        QLineF line(p1, p2);
        float angle = line.angle();

        QTransform transform;
        transform.rotate(-360.0 + angle);
        transform.translate(-p1.x(), -p1.y());

        QLineF transLine = transform.map(line);
        QPointF rotatedNoteCoord = transform.map(noteCoordinate);

        QPointF mappedBuffer = toNote.map(QPointF(0, buffer));
        QPointF origin = toNote.map(QPointF(0, 0));
        double bufferNoteCoords = QLineF(origin, mappedBuffer).length();

        p1 = transLine.p1();
        p2 = transLine.p2();

        p1 += QPointF(0.0, -bufferNoteCoords);
        p2 += QPointF(0.0, bufferNoteCoords);

        QRectF rect(p1, p2);
        if(rect.contains(rotatedNoteCoord)) {
            QPointF closestNoteCoord(rotatedNoteCoord.x(), rect.center().y());
            QTransform inverse = transform.inverted();
            closestNoteCoord = inverse.map(closestNoteCoord);
            return cwClosestPoint(closestNoteCoord, i+1);
        }
    }

    return cwClosestPoint();
}

/**
 * @brief cwBaseScrapInteraction::lastNoteTransformation
 * @return Will return the last note tranformation.  This will return nullptr
 * if the wasn't a previous note transformation from this trip
 */
cwNoteTranformation *cwBaseScrapInteraction::lastNoteTransformation() const
{
    cwNote* lastScrapNote = nullptr;
    if(note()->hasScraps()) {
        lastScrapNote = note();
    } else {
        cwSurveyNoteModel* model = note()->parentTrip()->notes();
        int index = model->notes().indexOf(note());
        int previousNote = index - 1;
        while(previousNote >= 0) {
            lastScrapNote = model->notes().at(previousNote);
            if(lastScrapNote->hasScraps()) {
                break;
            }
            previousNote--;
        }
    }

    if(lastScrapNote != nullptr) {
        if(lastScrapNote->hasScraps()) {
            cwScrap* lastScrap = lastScrapNote->scraps().last();
            if(!lastScrap->calculateNoteTransform()) {
                return lastScrap->noteTransformation();
            }
        }
    }

    return nullptr;
}

/**
 * @brief cwBaseScrapInteraction::deactivating
 *
 * Called when the interaction is not visible anymore
 */
void cwBaseScrapInteraction::deactivating() {
    if(!isVisible()) {
        startNewScrap();
    }
}

/**
  This starts a new scrap instead of
  */
void cwBaseScrapInteraction::startNewScrap() {
    closeCurrentScrap();
}


/**
Sets controlPointView
*/
void cwBaseScrapInteraction::setOutlinePointView(cwScrapOutlinePointView* controlPointView) {
    if(m_outlinePointView != controlPointView) {
        m_outlinePointView = controlPointView;
        emit outlinePointViewChanged();
    }
}

/**
  * @brief cwBaseScrapInteraction::setScrap
  * @param scrap - Sets the current scrap
  */
void cwBaseScrapInteraction::setScrap(cwScrap *scrap)
{
    if(m_scrap == scrap) { return; }

    if(m_scrap != nullptr) {
        disconnect(m_scrap, &cwScrap::destroyed, this, &cwBaseScrapInteraction::scrapDeleted);
    }

    m_scrap = scrap;

    if(m_scrap != nullptr) {
        connect(m_scrap, &cwScrap::destroyed, this, &cwBaseScrapInteraction::scrapDeleted);
    }

    emit scrapChanged();
}

cwScrapInteractionPoint cwBaseScrapInteraction::snapToScrapLine(QPoint imagePos) const
{
    cwScrapInteractionPoint interactionPoint;
    cwClosestPoint point = calcClosestPoint(imagePos);

    if(note()) {
        if (point.IsValid) {
            // Point is snapped
            interactionPoint.setIsSnapped(true);
            interactionPoint.setNoteCoordsPoint(point.ClosestPoint);

            auto toImage = cwScrapView::toImage(note());
            interactionPoint.setImagePoint(toImage.map(point.ClosestPoint));

            interactionPoint.setInsertIndex(point.InsertIndex);
        } else {
            // Point isn't snapped to the line
            interactionPoint.setIsSnapped(false);

            auto toNote = cwScrapView::toNormalized(note());
            interactionPoint.setNoteCoordsPoint(toNote.map(imagePos.toPointF()));

            interactionPoint.setImagePoint(imagePos);
            if (m_scrap == nullptr) {
                interactionPoint.setInsertIndex(0);
            } else {
                interactionPoint.setInsertIndex(m_scrap->numberOfPoints());
            }
        }
    }

    return interactionPoint;
}

