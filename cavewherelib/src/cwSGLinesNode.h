/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

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

    static double lineWidthFromZoom(double zoom, double lineWidth) {
        return lineWidth / zoom;
    }

private:
    float m_lineWidth;
    // QVector<QLineF> m_lines;
    // QVector<QPointF> m_lineStripPoints;
    QVector<QVector<QPointF>> m_lines;

    void setLineStrips(const QVector<QVector<QPointF>>& lines);
};

/**
 * @brief cwSGLinesNode::lineWidth
 * @return Get's the line width
 */
inline float cwSGLinesNode::lineWidth() const
{
    return m_lineWidth;
}


#endif // CWSGLINESNODE_H
