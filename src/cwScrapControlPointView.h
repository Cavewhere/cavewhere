#ifndef CWSCRAPCONTROLPOINTVIEW_H
#define CWSCRAPCONTROLPOINTVIEW_H

//Our includes
#include "cwAbstractPointManager.h"
#include "cwGlobalDirectory.h"
class cwScrap;

class cwScrapControlPointView : public cwAbstractPointManager
{
    Q_OBJECT

        Q_PROPERTY(cwScrap* scrap READ scrap WRITE setScrap NOTIFY scrapChanged)
public:
    explicit cwScrapControlPointView(QQuickItem *parent = 0);
    
    cwScrap* scrap() const;
    void setScrap(cwScrap* scrap);

protected:
    virtual QUrl qmlSource() const;
    virtual void updateItemData(QQuickItem* item, int pointIndex);
    virtual void updateItemPosition(QQuickItem* item, int pointIndex);

signals:
    void scrapChanged();
    
public slots:

private slots:

    void reset();

private:
    cwScrap* Scrap;

    
};

/**
 * @brief cwScrapControlPointView::scrap
 * @return Get's the current scrap of the control point view
 */
inline cwScrap *cwScrapControlPointView::scrap() const
{
    return Scrap;
}

/**
 * @brief cwScrapControlPointView::qmlSource
 * @return Returns the point's qml definition
 */
inline QUrl cwScrapControlPointView::qmlSource() const
{
    return cwGlobalDirectory::baseDirectory() + "qml/ScrapOutlinePoint.qml";
}


#endif // CWSCRAPCONTROLPOINTVIEW_H
