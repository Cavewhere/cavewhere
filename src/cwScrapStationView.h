#ifndef CWSCRAPSTATIONVIEW_H
#define CWSCRAPSTATIONVIEW_H

//Our includes
#include "cwScrapPointView.h"
#include "cwGlobalDirectory.h"
class cwTransformUpdater;
class cwScrap;
class cwImageItem;
class cwScrapItem;
class cwSGLinesNode;
#include "cwNoteStation.h"

//Qt includes
#include <QVariantAnimation>

/**
  This class manages a list of station items that visualize all the stations in a scrap.
  */
class cwScrapStationView : public cwScrapPointView
{
    Q_OBJECT

public:
    explicit cwScrapStationView(QQuickItem *parent = 0);
    ~cwScrapStationView();

    void setScrap(cwScrap* scrap);

    cwNoteStation selectedNoteStation() const;

signals:
    void scrapChanged();
    void scrapItemChanged();
    void shotLineScaleChanged();

public slots:

private:
    //Geometry for the shot lines
    bool LineDataDirty;
    cwSGLinesNode* ShotLinesNodeBackground;
    cwSGLinesNode* ShotLinesNodeForground;
    QVector<QLineF> ShotLines;

    QVariantAnimation* ScaleAnimation;

    cwTransformUpdater* OldTransformUpdater;

    virtual void updateItemPosition(QQuickItem* item, int index);
    virtual QUrl qmlSource() const;


private slots:
    void updateShotLinesWithAnimation();
    void updateShotLines();
    void updateTransformUpdate();

protected:
    virtual QSGNode* updatePaintNode(QSGNode * oldNode, UpdatePaintNodeData *);

};

Q_DECLARE_METATYPE(cwScrapStationView*)

/**
 * @brief cwScrapStationView::qmlSource
 * @return The qml source of the point that'll be rendered
 */
inline QUrl cwScrapStationView::qmlSource() const
{
    return QUrl::fromLocalFile(cwGlobalDirectory::baseDirectory() + "qml/NoteStation.qml");
}




#endif // CWSCRAPSTATIONVIEW_H
