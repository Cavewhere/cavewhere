#ifndef CWSGLINESNODE_H
#define CWSGLINESNODE_H

#include <QSGGeometryNode>

class cwSGLinesNode : public QSGGeometryNode
{
public:
    cwSGLinesNode();

    void setLines(const QVector<QLineF>& lines);
    void setLineStrip(const QVector<QPointF>& points);

    float lineWidth() const;
    void setLineWidth(float lineWidth);

private:
    float LineWidth;

};

/**
 * @brief cwSGLinesNode::lineWidth
 * @return Get's the line width
 */
inline float cwSGLinesNode::lineWidth() const
{
    return LineWidth;
}


#endif // CWSGLINESNODE_H
