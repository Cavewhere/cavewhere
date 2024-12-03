/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWSCALELENGTHITEM_H
#define CWSCALELENGTHITEM_H


//Our includes
#include "cwAbstract2PointItem.h"
class cwSGLinesNode;

//Qt includes
#include <QQmlEngine>

class cwScaleLengthItem : public cwAbstract2PointItem
{
    Q_OBJECT
    QML_NAMED_ELEMENT(ScaleLengthItem)

    Q_PROPERTY(double zoom READ zoom WRITE setZoom NOTIFY zoomChanged);

public:
    explicit cwScaleLengthItem(QQuickItem *parent = 0);
    
    double zoom() const { return m_zoom; }
    void setZoom(double zoom);

public slots:

signals:
    void zoomChanged();

protected:
    QSGNode* updatePaintNode(QSGNode *, UpdatePaintNodeData *);


private:
    void connectTransformer();
    void disconnectTransformer();

    void p1Updated();
    void p2Updated();

    //This should only be used in the rendering thread
    cwSGLinesNode* LinesNode;
    QVector<QLineF> Lines;

    //For scaling the scale bar correctly for drawing
    double m_zoom = 1.0;

private slots:
    void updateScaleLengthPath();

    QVector<QLineF> lengthLines() const;
    
};

/**
  This is called when the p1 is updated
  */
inline void cwScaleLengthItem::p1Updated() {
    updateScaleLengthPath();
}

/**
  This is called when p2 is updated
  */
inline void cwScaleLengthItem::p2Updated() {
    updateScaleLengthPath();
}

#endif // CWSCALELENGTHITEM_H
