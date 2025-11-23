/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWTRIANGULATETASK_H
#define CWTRIANGULATETASK_H

//Our includes
#include "cwTask.h"
#include "cwTriangulateInData.h"
#include "cwTriangulatedData.h"
#include "cwImage.h"
#include "cwNoteTranformation.h"
#include "cwTextureUploadTask.h"
#include "cwTriangulateLiDARInData.h"
class cwCropImageTask;

//Qt include
#include <QPolygonF>
#include <QVector>
#include <QVector3D>
#include <QSet>
#include <QPoint>

class cwTriangulateTask
{
//    Q_OBJECT
public:
    explicit cwTriangulateTask() = default;
    
    //Input so the triangle task
    void setScrapData(QList<cwTriangulateInData> scraps);
    void setProjectFilename(QString filename);
    void setFormatType(cwTextureUploadTask::Format format);

    //Outputs of the task
    QList<QFuture<cwTriangulatedData> > triangulate() const;

    static QVector3D morphPoint(const QList<cwTriangulateStation>& visibleStations,
                                const QMatrix4x4 &toWorldCoords,
                                const QMatrix4x4 &viewMatrix,
                                const QVector3D &point);

    template<typename T>
    static QList<cwTriangulateStation> buildStationsWithInterpolatedShots(const T& data) {
        const auto stationLookup = data.stationLookup();

        if(stationLookup.isEmpty()) {
            return QList<cwTriangulateStation>();
        }

        QList<cwTriangulateStation> stations;
        stations.reserve(data.noteStations().size());

        const QHash<QString, QVector3D> notePositions = [&]() {
            QHash<QString, QVector3D> notePositions;
            notePositions.reserve(data.noteStations().size());

            for (const auto& station : data.noteStations()) {
                const QString stationName = station.name();
                const QString normalizedName = stationName.toLower();
                if(stationLookup.hasPosition(normalizedName)) [[likely]] {
                    notePositions.insert(normalizedName, QVector3D(station.positionOnNote()));
                    stations.append(cwTriangulateStation(
                        stationName,
                        QVector3D(station.positionOnNote()),
                        stationLookup.position(stationName)));
                }
            }

            return notePositions;
        }();

        const auto network = data.surveyNetwork();

        //This could potentially be a parameter
        constexpr double kDummySpacingMeters = 1.0;
        QSet<QString> processedShots;

        const auto noteStations = data.noteStations();
        for(const auto& noteStation : noteStations) {
            const QString from = noteStation.name();

            if(!stationLookup.hasPosition(from)) [[unlikely]] {
                continue;
            }

            const QVector3D fromNote = QVector3D(noteStation.positionOnNote());
            const QVector3D fromWorld = stationLookup.position(from);

            for (const QString& toStation : network.neighbors(from)) {
                if(!notePositions.contains(toStation)) {
                    continue;
                }

                const QString to = toStation.toLower();
                QString shotKey = from < to ? from + "|" + to : to + "|" + from;
                if (processedShots.contains(shotKey)) {
                    continue;
                }
                processedShots.insert(shotKey);

                Q_ASSERT(stationLookup.hasPosition(to));

                const QVector3D toNote = notePositions.value(to);
                const QVector3D toWorld = stationLookup.position(to);

                const double shotLength = static_cast<double>((toWorld - fromWorld).length());
                if (shotLength <= 0.0) {
                    continue;
                }

                const int segmentCount = static_cast<int>(std::ceil(shotLength / kDummySpacingMeters));
                if (segmentCount <= 1) {
                    continue;
                }

                const QVector3D noteStep = (toNote - fromNote) / static_cast<float>(segmentCount);
                const QVector3D worldStep = (toWorld - fromWorld) / static_cast<float>(segmentCount);

                for (int i = 1; i < segmentCount; ++i) {
                    const float t = static_cast<float>(i);
                    QVector3D notePos = fromNote + noteStep * t;
                    QVector3D worldPos = fromWorld + worldStep * t;
                    const QString dummyName = QString("%1_%2_%3").arg(from, to).arg(i);
                    stations.append(cwTriangulateStation(dummyName, notePos, worldPos));
                }
            }
        }

        return stations;
    }

signals:
    
public slots:

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
        QSizeF GridDeltaSize; //In PointsPerMeter

        bool intersects(const Quad& quad, const QPolygonF &polygon) const;
        bool quadContainInsideOfPolygon(const Quad& quad, const QPolygonF& polygon) const;

        Quad quad(int origin) const;
        int index(int x, int y) const;
        int index(QPointF point) const;
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

    /**
     *
     */
    struct ScrapGeomtery {
        QVector<QVector3D> points;
        QVector<quint32> indices;
    };

    //Inputs
    QList<cwTriangulateInData> Scraps;
    QString ProjectFilename;
    cwTextureUploadTask::Format Format;

    //Outputs
    QList<cwTriangulatedData> TriangulatedScraps;


    static QFuture<cwTrackedImagePtr> cropScrap(const cwTriangulateInData& scrap,
                                                const QString& projectFilename,
                                                cwTextureUploadTask::Format format);

    static cwTriangulatedData triangulateGeometry(const cwTriangulateInData& scrap,
                                                  cwTrackedImagePtr croppedImage,
                                                  const cwTextureUploadTask::UploadResult &imageData);

    static PointGrid createPointGrid(QRectF bounds, const cwTriangulateInData& scrapData);
    static QSet<int> pointsInPolygon(const PointGrid& grid, const QPolygonF& polygon);
    static QuadDatabase createQuads(const PointGrid& grid, const QPolygonF& polygon);

    //For triangulation
    static ScrapGeomtery createTriangles(const PointGrid& grid, const QSet<int> pointsInOutline, const QuadDatabase& database, const cwTriangulateInData& inScrapData);
    static QVector<uint32_t> createTrianglesFull(const QuadDatabase& database, const QHash<int, int>& mapGridToOut);
    static QVector<QPointF> createTrianglesPartial(const PointGrid& grid, const QuadDatabase &database, const QPolygonF& scrapOutline);
    static QPolygonF addPointsOnOverlapingEdges(QPolygonF polygon);
    static QList<QPolygonF> createSimplePolygons(QPolygonF polygon);
    static void mergeFullAndPartialTriangles(QVector<QVector3D>& pointSet, QVector<uint32_t>& indices, const QVector<QPointF>& unAddedTriangles);

    //For transformation from note coords to local note coords
    static QMatrix4x4 mapToScrapCoordinates(const QRectF& bounds);
    static QVector<QVector3D> mapToLocalNoteCoordinates(QMatrix4x4 toLocal, const QVector<QVector3D>& normalizeNoteCoords);
    static QVector<QVector2D> mapTexCoordinates(const QVector<QVector3D>& normalizeNoteCoords);
    static QVector<QVector2D> scaleTexCoordinates(const cwImage& image, QVector<QVector2D> texCoords);

    //For morphing
    static QVector<QVector3D> morphPoints(const QVector<QVector3D> &notePoints,
                                          const cwTriangulateInData &scrapData,
                                          const QMatrix4x4& toLocal,
                                          const cwImage& croppedImage);
    static QList<cwTriangulateStation> stationsVisibleToPoint(const QVector3D& point,
                                                              const QList<cwTriangulateStation>& stations,
                                                              const QPolygonF& scrapOutline);
    // static QVector3D morphPoint(const QList<cwTriangulateStation>& visibleStations,
    //                             const QMatrix4x4 &toWorldCoords,
    //                             const QMatrix4x4 &viewMatrix,
    //                             const QVector3D &point);

    //For lead handling
    static QVector<QVector3D> leadPositionToVector3D(const QList<cwLead>& leads);

    static QMatrix4x4 toNoteMatrix(const cwNoteTransformationData& data);
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
    return QPoint(index % GridSize.width(), index / GridSize.width());
}


/**
  Retruns true if the index is in the point grid and false if it's not.
  */
inline bool cwTriangulateTask::PointGrid::isValid(int index) const {
    return index >= 0 && index < Points.size();
}

#endif // CWTRIANGULATETASK_H
