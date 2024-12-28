/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWBASESCRAPINTERACTION_H
#define CWBASESCRAPINTERACTION_H

//Our includes
#include "cwNoteInteraction.h"
#include "cwScrap.h"
#include "cwScrapInteractionPoint.h"
#include "cwScrapOutlinePointView.h"
class cwNoteTranformation;
class cwNoteCamera;

//Qt includes
#include <QQmlEngine>

class cwBaseScrapInteraction : public cwNoteInteraction
{
    Q_OBJECT
    QML_NAMED_ELEMENT(BaseScrapInteraction)

    // Q_PROPERTY(cwImageItem* imageItem READ imageItem WRITE setImageItem NOTIFY imageItemChanged)
    // Q_PROPERTY(cwNoteCamera* noteCamera READ noteCamera WRITE setNoteCamera NOTIFY noteCameraChanged)

    Q_PROPERTY(cwScrapOutlinePointView* outlinePointView READ outlinePointView WRITE setOutlinePointView NOTIFY outlinePointViewChanged REQUIRED)
    Q_PROPERTY(cwScrap* scrap READ scrap WRITE setScrap NOTIFY scrapChanged REQUIRED)

    //Note is in the base class
    // Q_PROPERTY(cwNote* note READ note WRITE setNote NOTIFY noteChanged REQUIRED);

public:
    explicit cwBaseScrapInteraction(QQuickItem *parent = 0);

//    Q_INVOKABLE void addPoint(QPoint imageCoordinate);

    // cwImageItem* imageItem() const;
    // void setImageItem(cwImageItem* imageItem);

    cwScrapOutlinePointView* outlinePointView() const;
    void setOutlinePointView(cwScrapOutlinePointView* outlinePointView);

    Q_INVOKABLE void addScrap();
    void setScrap(cwScrap* scrap);
    cwScrap* scrap() const;

    // cwNoteCamera* noteCamera() const;
    // void setNoteCamera(cwNoteCamera* noteCamera);

    Q_INVOKABLE cwScrapInteractionPoint snapToScrapLine(QPoint imagePos) const;

signals:
    // void imageItemChanged();
    // void noteCameraChanged();
    void outlinePointViewChanged();
    void scrapChanged();
    void noteChanged();

public slots:
    void startNewScrap();

private:
    class cwClosestPoint {
    public:
        cwClosestPoint() : IsValid(false), InsertIndex(-1) { }
        cwClosestPoint(QPointF closestPoint, int insertIndex) :
            IsValid(true),
            ClosestPoint(closestPoint),
            InsertIndex(insertIndex) { }

        bool IsValid;
        QPointF ClosestPoint;
        int InsertIndex;
    };

    cwScrap* m_scrap;
    // cwNoteCamera* m_noteCamera;
    // cwImageItem* ImageItem; //!< For converting viewport coordinates into note coordinates
    cwScrapOutlinePointView* m_outlinePointView; //!< For testing if we clicked on a control point

    void closeCurrentScrap();

    cwClosestPoint calcClosestPoint(QPoint qtViewportPosition) const;

    cwNoteTranformation* lastNoteTransformation() const;

private slots:
    void deactivating();
    void scrapDeleted();

};



// #include "cwNoteCamera.h"

/**
Gets imageItem
*/
// inline cwImageItem* cwBaseScrapInteraction::imageItem() const {
//     return ImageItem;
// }

/**
Gets controlPointView
*/
inline cwScrapOutlinePointView* cwBaseScrapInteraction::outlinePointView() const {
    return m_outlinePointView;
}

/**
 * @brief cwBaseScrapInteraction::scrap
 * @return The current scrap of this interaction
 */
inline cwScrap *cwBaseScrapInteraction::scrap() const
{
    return m_scrap;
}

// inline cwNoteCamera *cwBaseScrapInteraction::noteCamera() const {
//     return m_noteCamera;
// }

/**
 * @brief cwBaseScrapInteraction::scrapDeleted
 *
 * Called when the current scrap is destroyed
 */
inline void cwBaseScrapInteraction::scrapDeleted() {
    m_scrap = nullptr;
}


#endif // CWBASESCRAPINTERACTION_H
