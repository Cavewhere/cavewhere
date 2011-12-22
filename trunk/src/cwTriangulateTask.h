#ifndef CWTRIANGULATETASK_H
#define CWTRIANGULATETASK_H

//Our includes
#include "cwTask.h"
#include "cwTriangulateInData.h"
#include "cwTriangulatedData.h"
#include "cwImage.h"
#include "cwNoteTranformation.h"
class cwCropImageTask;

//Qt include
#include <QPolygonF>
#include <QVector>
#include <QVector3D>
#include <QSet>
#include <QPoint>

class cwTriangulateTask : public cwTask
{
    Q_OBJECT
public:
    explicit cwTriangulateTask(QObject *parent = 0);
    
    //Input so the triangle task
    void setScrapData(QList<cwTriangulateInData> scraps);
    void setProjectFilename(QString filename);

    //Outputs of the task
    QList<cwTriangulatedData> triangulatedScrapData() const;

signals:
    
public slots:

protected:
    virtual void runTask();


private:

    /**
      The quad structure

       p0 --- p1
       |      |
       |      |
       p2 --- p3
      */
    class Quad {
    public:
        Quad() { TopLeft = TopRight = BottomLeft = BottomRight = -1; }
        Quad(int topLeft, int topRight, int bottomLeft, int bottomRight) :
            TopLeft(topLeft),
            TopRight(topRight),
            BottomLeft(bottomLeft),
            BottomRight(bottomRight) { }

        int topLeft() const { return TopLeft; }
        int topRight()  const { return TopRight; }
        int bottomLeft() const { return BottomLeft; }
        int bottomRight() const { return BottomRight; }

    private:
        int TopLeft;
        int TopRight;
        int BottomLeft;
        int BottomRight;
    };

    /**
        A regualar point grid.  Points should be equaly space for x and y directions
      */
    class PointGrid {
    public:
        QSize GridSize;
        QVector<QPointF> Points;

        Quad quad(int origin) const;
        int index(int x, int y) const;
        QPoint xyIndices(int) const;
        bool isValid(int index) const;
    };

    /**
      This stores two set of indices:
      1. Origin indices of full quads
      2. Origin indices of partial quads

      A full quad is where all 4 points in the quad are in the scrap's outline
      A partial quad is where 1 to 3 points are in the scrap (the quad is on the scrap's edge of the outline)

       The origin index of the quad is always to top left corner of the quad.

       A quad is made up 4 points that are xy axis align

       p0 --- p1
       |      |
       |      |
       p2 --- p3

       0 is the origin.
    */
    class QuadDatabase {
    public:
        QList<Quad> FullQuads;
        QList<Quad> PartialQuads;
    };

    //Inputs
    QList<cwTriangulateInData> Scraps;
    QString ProjectFilename;

    //Outputs
    QList<cwTriangulatedData> TriangulatedScraps;

    //Sub tasks
    cwCropImageTask* CropTask;

    void cropScraps();

    void triangulateScraps();
    void triangulateScrap(int index);
    PointGrid createPointGrid(QRectF bounds, cwImage scrapImage, const cwNoteTranformation &noteTransform) const;
    QSet<int> pointsInPolygon(const PointGrid& grid, const QPolygonF& polygon) const;
    QuadDatabase createQuads(const PointGrid& grid, const QSet<int>& pointsInScrap);

    void createTriangles(const PointGrid& grid, const QSet<int> pointsInOutline, const QuadDatabase& database, int index);
    QVector<uint> createTrianglesFull(const QuadDatabase& database, const QHash<int, int>& mapGridToOut);
    QVector<QPointF> createTrianglesPartial(const PointGrid& grid, const QuadDatabase &database, const QPolygonF& scrapOutline);
    void mergeFullAndPartialTriangles(QVector<QVector3D>& pointSet, QVector<uint>& indices, const QVector<QPointF>& unAddedTriangles);

};

/**
  \brief Get's the index at x, y in the grid
  */
inline int cwTriangulateTask::PointGrid::index(int x, int y) const {
    return y * GridSize.width() + x;
}

/**
  \brief Gets the x and y indices at index of the grid

  This is useful for converting an index into x, y indices
  */
inline QPoint cwTriangulateTask::PointGrid::xyIndices(int index) const {
    return QPoint(index / GridSize.width(), index % GridSize.width());
}

/**
  Retruns true if the index is in the point grid and false if it's not.
  */
inline bool cwTriangulateTask::PointGrid::isValid(int index) const {
    return index >= 0 && index < Points.size();
}

#endif // CWTRIANGULATETASK_H
