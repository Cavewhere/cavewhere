/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWPASSAGETUBEGALLERYMODEL_H
#define CWPASSAGETUBEGALLERYMODEL_H

//Our includes
#include "CaveWhereLibExport.h"
#include "cwLinePlotGeometry.h"
#include "cwPassageTubeCases.h"
#include "cwPassageTubeMesh.h"
#include "cwPassageTubeMeshCheck.h"

//Qt includes
#include <QAbstractListModel>
#include <QBox3D>
#include <QMatrix4x4>
#include <QPointF>
#include <QPointer>
#include <QQmlEngine>
#include <QStringList>
#include <QVector3D>
#include <QVector>

//Std includes
#include <optional>
#include <variant>

class cwLinePlotManager;
class cwProject;
class cwRenderLinePlot;
class cwRenderPassageTube;
class cwScene;

/**
 * The tube gallery's list of surveys: the cwPassageTubeCases catalog, seeded
 * random caves and the caves of opened CaveWhere projects. Selecting a row builds it with
 * cwPassageTubeBuilder::buildDebug and checks it with
 * cwPassageTubeMeshCheck on the GUI thread; the mesh and centerline go to a
 * tube and a line plot the model adds to its scene. A second line plot draws
 * the builder's rings and their frames.
 *
 * Issue counts for the list are built on first read and cached.
 *
 * openProject loads a copy of a project in a temporary directory, solves its
 * line plot and keeps each cave's survey and station positions as a row; the
 * copy and the project are dropped once the rows are in.
 */
class CAVEWHERE_LIB_EXPORT cwPassageTubeGalleryModel : public QAbstractListModel
{
    Q_OBJECT
    QML_NAMED_ELEMENT(PassageTubeGalleryModel)

    Q_PROPERTY(int count READ count NOTIFY countChanged)
    Q_PROPERTY(bool includeSweep READ includeSweep WRITE setIncludeSweep NOTIFY includeSweepChanged)
    Q_PROPERTY(int currentIndex READ currentIndex WRITE setCurrentIndex NOTIFY currentIndexChanged)
    Q_PROPERTY(cwScene* scene READ scene WRITE setScene NOTIFY sceneChanged)
    Q_PROPERTY(cwRenderPassageTube* tube READ tube NOTIFY sceneChanged)
    Q_PROPERTY(cwRenderLinePlot* linePlot READ linePlot NOTIFY sceneChanged)
    Q_PROPERTY(cwRenderLinePlot* ringPlot READ ringPlot NOTIFY sceneChanged)
    Q_PROPERTY(bool ringsVisible READ ringsVisible WRITE setRingsVisible NOTIFY ringsVisibleChanged)
    Q_PROPERTY(bool framesVisible READ framesVisible WRITE setFramesVisible NOTIFY framesVisibleChanged)
    Q_PROPERTY(QString station READ station WRITE setStation NOTIFY stationChanged)

    Q_PROPERTY(QString currentName READ currentName NOTIFY currentChanged)
    Q_PROPERTY(QString parameters READ parameters NOTIFY currentChanged)
    Q_PROPERTY(QString reportText READ reportText NOTIFY currentChanged)
    Q_PROPERTY(QVector<QVector3D> issuePositions READ issuePositions NOTIFY currentChanged)
    Q_PROPERTY(QVector<QVector3D> warningPositions READ warningPositions NOTIFY currentChanged)
    Q_PROPERTY(QStringList issueTexts READ issueTexts NOTIFY currentChanged)
    Q_PROPERTY(QVector3D boundsCenter READ boundsCenter NOTIFY currentChanged)
    Q_PROPERTY(double boundsRadius READ boundsRadius NOTIFY currentChanged)
    Q_PROPERTY(QBox3D framingBox READ framingBox NOTIFY currentChanged)
    Q_PROPERTY(QVector3D junction READ junction NOTIFY currentChanged)
    Q_PROPERTY(QVector3D armFarEnd READ armFarEnd NOTIFY currentChanged)
    Q_PROPERTY(QVector3D armDirection READ armDirection NOTIFY currentChanged)

public:
    enum Role {
        NameRole = Qt::UserRole + 1,
        TagsRole,
        IssueCountRole,
        IssueSummaryRole
    };
    Q_ENUM(Role)

    // How far a ring's frame cross reaches from its center along right and up;
    // an elbow ring's reaches kElbowFrameScale times as far.
    static constexpr double kFrameAxisMeters = 0.5;
    static constexpr double kElbowFrameScale = 2.0;

    // The rings of every chain as a line list. Each ring's loop runs through
    // its vertices back to the first; a grown ring draws every other edge.
    // rights and ups hold one segment per ring, in loop order, from its center
    // along frame.right and frame.up.
    struct RingGeometry {
        QVector<QVector3D> points;
        QVector<cwLinePlotGeometry::VertexRange> loops;
        cwLinePlotGeometry::VertexRange rights;
        cwLinePlotGeometry::VertexRange ups;
    };

    explicit cwPassageTubeGalleryModel(QObject* parent = nullptr);
    ~cwPassageTubeGalleryModel() override;

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    int count() const;

    // Sweep cases are left out of the list until this is set.
    bool includeSweep() const;
    void setIncludeSweep(bool includeSweep);

    int currentIndex() const;
    void setCurrentIndex(int currentIndex);

    cwScene* scene() const;
    void setScene(cwScene* scene);

    cwRenderPassageTube* tube() const;
    cwRenderLinePlot* linePlot() const;
    cwRenderLinePlot* ringPlot() const;

    bool ringsVisible() const;
    void setRingsVisible(bool ringsVisible);

    bool framesVisible() const;
    void setFramesVisible(bool framesVisible);

    // The station the camera frames, matched ignoring case; empty, or a
    // station the current row lacks, frames the whole survey.
    QString station() const;
    void setStation(const QString& station);

    // Adds a cwPassageTubeCases::random survey, or a randomWild one when wild.
    Q_INVOKABLE void addRandom(int seed, int junctionCount, bool wild = false);

    // Loads a copy of the .cw or .cwproj at path and appends a row per cave,
    // named "<project>: <cave>" and built at sampleSpacing meters. Emits
    // projectOpened with the new rows, or projectFailed.
    Q_INVOKABLE void openProject(const QString& path, double sampleSpacing);

    // The row named name, else the first whose name starts with it ignoring
    // case, else -1.
    Q_INVOKABLE int indexOf(const QString& name) const;

    // The rows whose mesh check reports issues, building each unknown count.
    Q_INVOKABLE QList<int> failingRows();

    QString currentName() const;
    QString parameters() const;
    QString reportText() const;
    QVector<QVector3D> issuePositions() const;
    QVector<QVector3D> warningPositions() const;
    QStringList issueTexts() const;
    QVector3D boundsCenter() const;
    double boundsRadius() const;

    // The legs at station() and their readings, else the mesh and centerline
    // bounds, a unit cube when the case is empty.
    QBox3D framingBox() const;

    // station(), else the first station where three or more passages meet,
    // the bounds center when there is none.
    QVector3D junction() const;

    // The far station of station()'s longest leg, else of the first arm into
    // junction(); and the horizontal unit direction from it toward junction().
    QVector3D armFarEnd() const;
    QVector3D armDirection() const;

    cwPassageTubeMesh mesh() const;
    cwPassageTubeMeshCheck::Report report() const;
    RingGeometry ringGeometry() const;

    // Rings are numbered across chains in chain order. ringAt is the ring
    // whose center lies nearest point on screen, within maxPixels, else -1.
    Q_INVOKABLE int ringAt(QPointF point, const QMatrix4x4& modelToViewport, double maxPixels) const;
    Q_INVOKABLE int nearestRing(const QVector3D& position) const;

    // The ring's chain, index, leg, distance to its station, frame up,
    // inclination weight, radius range and kind; empty for an unknown ring.
    Q_INVOKABLE QString ringLine(int ring) const;

signals:
    void countChanged();
    void includeSweepChanged();
    void currentIndexChanged();
    void sceneChanged();
    void currentChanged();
    void ringsVisibleChanged();
    void framesVisibleChanged();
    void stationChanged();
    void projectOpened(int firstRow, int rowCount);
    void projectFailed(const QString& message);

private:
    // A catalog or random case, or one cave of an opened project.
    struct Row {
        QString name;
        QStringList tags;
        double sampleSpacing = 0.0;
        std::variant<cwPassageTubeCases::Case, cwPassageTubeCases::Survey> source;
        mutable std::optional<int> issueCount;
        mutable QString issueSummary;

        cwPassageTubeCases::Survey survey() const;
    };

    struct StationFrame {
        QBox3D box;
        QVector3D station;
        QVector3D farEnd;
    };

    struct Build {
        cwPassageTubeBuilder::Debug debug;
        cwPassageTubeMeshCheck::Report report;
    };

    static Row caseRow(const cwPassageTubeCases::Case& entry);
    static Build build(const cwPassageTubeCases::Survey& survey, double sampleSpacing);
    static QString summary(const cwPassageTubeMeshCheck::Report& report);

    void loadCatalog();
    void appendRows(const QVector<Row>& rows);
    void cacheIssues(const Row& row, const cwPassageTubeMeshCheck::Report& report) const;
    void ensureIssueCount(const Row& row) const;
    void selectCurrent();
    void locateJunction(const cwPassageTubeCases::Survey& survey);
    void locateStation();
    void collectWhenSolved(cwProject* project, cwLinePlotManager* linePlot, const QString& label, double sampleSpacing);
    void addProjectRows(cwProject* project, const QString& label, double sampleSpacing);
    void failProject(cwProject* project, const QString& message);
    void dropProject(cwProject* project);
    void pushToScene();
    void applyRingVisibility();
    void deleteRenderObjects();

    QVector<Row> m_rows;
    QVector<Row> m_addedRows;
    QList<cwProject*> m_loadingProjects;
    bool m_includeSweep = false;
    int m_currentIndex = -1;

    QPointer<cwScene> m_scene;
    QPointer<cwRenderPassageTube> m_tube;
    QPointer<cwRenderLinePlot> m_linePlot;
    QPointer<cwRenderLinePlot> m_ringPlot;
    bool m_ringsVisible = false;
    bool m_framesVisible = false;
    QString m_station;

    Build m_current;
    RingGeometry m_ringGeometry;
    QBox3D m_bounds;
    QVector3D m_junction;
    QVector3D m_armFarEnd;
    std::optional<StationFrame> m_stationFrame;
};

#endif // CWPASSAGETUBEGALLERYMODEL_H
