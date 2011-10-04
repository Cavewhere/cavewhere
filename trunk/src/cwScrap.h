#ifndef CWSCRAP_H
#define CWSCRAP_H

//Qt includes
#include <QObject>
#include <QVector2D>
#include <QVector>

/**
  cwScrap holds a polygon of points that represents a scrap

  Points can be added or removed from the scrap.  All the points will be in
  normalize note coordinates system.
  */
class cwScrap : public QObject
{
    Q_OBJECT
public:
    explicit cwScrap(QObject *parent = 0);

    void addPoint(QPointF point);
    void insertPoint(int index, QPointF point);
    void removePoint(int index);
    QVector<QPointF> points();
    int numberOfPoints() const;

signals:
    void insertedPoints(int begin, int end);
    void removedPoints(int begin, int end);

private:
    QVector<QPointF> Points;

};

Q_DECLARE_METATYPE(cwScrap*)

/**
  \brief Adds a point to the scrap at the end of the scrap line
  */
inline void cwScrap::addPoint(QPointF point) {
    insertPoint(Points.size(), point);
}

/**
  \brief Gets all the bound points in the scrap
  */
inline QVector<QPointF> cwScrap::points() {
    return Points;
}

/**
  \brief Gets number of points in the scrap
  */
inline int cwScrap::numberOfPoints() const {
    return Points.size();
}

#endif // CWSCRAP_H
