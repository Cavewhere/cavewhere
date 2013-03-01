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

/**
  This class manages a list of station items that visualize all the stations in a scrap.
  */
class cwScrapStationView : public cwScrapPointView
{
    Q_OBJECT

    Q_PROPERTY(float shotLineScale READ shotLineScale WRITE setShotLineScale NOTIFY shotLineScaleChanged)

public:
    explicit cwScrapStationView(QQuickItem *parent = 0);
    ~cwScrapStationView();

    float shotLineScale() const;
    void setShotLineScale(float scale);

    void setScrap(cwScrap* scrap);

    cwNoteStation selectedNoteStation() const;

signals:
    void scrapChanged();
    void scrapItemChanged();
    void shotLineScaleChanged();

public slots:

private:
    //Geometry for the shot lines
    cwSGLinesNode* ShotLinesNode;
    QVector<QLineF> ShotLines;

    float ShotLineScale;

    cwTransformUpdater* OldTransformUpdater;

    virtual void updateItemPosition(QQuickItem* item, int index);
    virtual QUrl qmlSource() const;


private slots:
    void updateShotLines();
    void updateTransformUpdate();

protected:
    virtual QSGNode* updatePaintNode(QSGNode * oldNode, UpdatePaintNodeData *);

};

Q_DECLARE_METATYPE(cwScrapStationView*)


/**
 * @brief cwScrapStationView::shotLineScale
 * @return Return's the shot line scale, 0.0 to 1.0, useful for animating the shot lines
 */
inline float cwScrapStationView::shotLineScale() const {
    return ShotLineScale;
}

/**
 * @brief cwScrapStationView::qmlSource
 * @return The qml source of the point that'll be rendered
 */
inline QUrl cwScrapStationView::qmlSource() const
{
    return QUrl::fromLocalFile(cwGlobalDirectory::baseDirectory() + "qml/NoteStation.qml");
}


#endif // CWSCRAPSTATIONVIEW_H
