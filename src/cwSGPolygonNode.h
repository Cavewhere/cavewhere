#ifndef CWSGPOLYGONNODE_H
#define CWSGPOLYGONNODE_H

#include <QSGGeometryNode>
#include <QPolygonF>

class cwSGPolygonNode : public QSGGeometryNode
{
public:
    cwSGPolygonNode();

    void setPolygon(const QPolygonF& polygon);
};

#endif // CWSGPOLYGONNODE_H
