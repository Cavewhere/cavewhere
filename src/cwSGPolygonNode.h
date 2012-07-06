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
