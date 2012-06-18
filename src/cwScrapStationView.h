#ifndef CWSCRAPSTATIONVIEW_H
#define CWSCRAPSTATIONVIEW_H

//Our includes
#include "cwAbstractPointManager.h"
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
class cwScrapStationView : public cwAbstractPointManager
{
    Q_OBJECT

    Q_PROPERTY(cwScrap* scrap READ scrap WRITE setScrap NOTIFY scrapChanged)
    Q_PROPERTY(cwScrapItem* scrapItem READ scrapItem WRITE setScrapItem NOTIFY scrapItemChanged)
    Q_PROPERTY(float shotLineScale READ shotLineScale WRITE setShotLineScale NOTIFY shotLineScaleChanged)

public:
    explicit cwScrapStationView(QQuickItem *parent = 0);
    ~cwScrapStationView();

    cwScrap* scrap() const;
    void setScrap(cwScrap* scrap);

    float shotLineScale() const;
    void setShotLineScale(float scale);

    cwNoteStation selectedNoteStation() const;

    cwScrapItem* scrapItem() const;
    void setScrapItem(cwScrapItem* scrapItem);

signals:
    void scrapChanged();
    void scrapItemChanged();
    void shotLineScaleChanged();

public slots:

private:
    cwScrap* Scrap; //!< The scrap this is class keeps track of

    //Geometry for the shot lines
    cwSGLinesNode* ShotLinesNode;
    QVector<QLineF> ShotLines;

    float ShotLineScale;

    cwScrapItem* ScrapItem; //!< For selection and holding the scrap

    virtual void updateItemData(QQuickItem* item, int index);
    virtual void updateItemPosition(QQuickItem* item, int index);
    virtual QUrl qmlSource() const;


private slots:
    void updateShotLines();

protected:
    virtual QSGNode* updatePaintNode(QSGNode * oldNode, UpdatePaintNodeData *);

};

Q_DECLARE_METATYPE(cwScrapStationView*)

/**
    Gets scrap that this class renders all the stations of
*/
inline cwScrap* cwScrapStationView::scrap() const {
    return Scrap;
}

/**
  Gets scrapItem
  */
inline cwScrapItem* cwScrapStationView::scrapItem() const {
    return ScrapItem;
}

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
    return cwGlobalDirectory::baseDirectory() + "qml/NoteStation.qml";
}


#endif // CWSCRAPSTATIONVIEW_H
