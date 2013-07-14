/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWSGPOLYGONNODE_H
#define CWSGPOLYGONNODE_H

#include <QSGGeometryNode>
#include <QPolygonF>

class cwSGPolygonNode : public QSGGeometryNode
{
public:
    cwSGPolygonNode();

    void setPolygon(const QPolygonF& polygon);
    void setColor(const QColor& color);
};

#endif // CWSGPOLYGONNODE_H
