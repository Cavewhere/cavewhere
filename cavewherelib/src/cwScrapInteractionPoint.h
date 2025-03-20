#ifndef CWSCRAPINTERACTIONPOINT_H
#define CWSCRAPINTERACTIONPOINT_H

#include <QObject>
#include <QPointF>
#include <QQmlEngine>

class cwScrapInteractionPoint {
    Q_GADGET
    QML_NAMED_ELEMENT(scrapInteractionPoint)

    Q_PROPERTY(bool isSnapped READ isSnapped WRITE setIsSnapped)
    Q_PROPERTY(QPointF noteCoordsPoint READ noteCoordsPoint WRITE setNoteCoordsPoint)
    Q_PROPERTY(QPointF imagePoint READ imagePoint WRITE setImagePoint)
    Q_PROPERTY(int insertIndex READ insertIndex WRITE setInsertIndex)

public:
    // Inline getter and setter for isSnapped
    bool isSnapped() const { return m_isSnapped; }
    void setIsSnapped(bool value) { m_isSnapped = value; }

    // Inline getter and setter for noteCoordsPoint
    QPointF noteCoordsPoint() const { return m_noteCoordsPoint; }
    void setNoteCoordsPoint(const QPointF& point) { m_noteCoordsPoint = point; }

    // Inline getter and setter for imagePoint
    QPointF imagePoint() const { return m_imagePoint; }
    void setImagePoint(const QPointF& point) { m_imagePoint = point; }

    // Inline getter and setter for insertIndex
    int insertIndex() const { return m_insertIndex; }
    void setInsertIndex(int index) { m_insertIndex = index; }

private:
    bool m_isSnapped = false;
    QPointF m_noteCoordsPoint;
    QPointF m_imagePoint;
    int m_insertIndex = -1;
};
#endif // CWSCRAPINTERACTIONPOINT_H
