#ifndef CWSGLINESNODE_H
#define CWSGLINESNODE_H

#include <QSGGeometryNode>

class cwSGLinesNode : public QSGGeometryNode
{
public:
    cwSGLinesNode();

    void setLines(const QVector<QLineF>& lines);
};

#endif // CWSGLINESNODE_H
