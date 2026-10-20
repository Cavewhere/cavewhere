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
#include "cwPassageTubeSandboxModel.h"

//Qt includes
#include <QAbstractListModel>
#include <QBox3D>
#include <QColor>
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
class QTimer;

/**
 * The tube gallery's list of surveys: the cwPassageTubeCases catalog, seeded
 * random caves and the caves of opened CaveWhere projects. Selecting a row builds it with
 * cwPassageTubeBuilder::buildDebug and checks it with
 * cwPassageTubeMeshCheck on the GUI thread; the mesh and centerline go to a
 * tube and a line plot the model adds to its scene. A second line plot draws
 * the builder's rings and their frames, the survey's readings, the sections
 * the rings were fitted to, and where a ring reaches past its section.
 *
 * Issue counts for the list are built on first read and cached.
 *
 * Row kSandboxRow, named "Sandbox", builds the sandbox's junction case. Its
 * edits rebuild it, debounced to the last edit within one frame, while it is
 * the current row; a build over kSandboxBuildBudgetMilliseconds doubles the
 * sample spacing of the sandbox's next build, up to
 * kSandboxMaxSampleSpacingMeters, until the sandbox loads or its spacing is
 * set. Rows from firstCaseRow on are the catalog and added cases.
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
    Q_PROPERTY(bool readingsVisible READ readingsVisible WRITE setReadingsVisible NOTIFY readingsVisibleChanged)
    Q_PROPERTY(bool sectionsVisible READ sectionsVisible WRITE setSectionsVisible NOTIFY sectionsVisibleChanged)
    Q_PROPERTY(bool overshootVisible READ overshootVisible WRITE setOvershootVisible NOTIFY overshootVisibleChanged)
    Q_PROPERTY(QString station READ station WRITE setStation NOTIFY stationChanged)
    Q_PROPERTY(bool rawTubesVisible READ rawTubesVisible WRITE setRawTubesVisible NOTIFY rawTubesVisibleChanged)
    Q_PROPERTY(bool capsSlid READ capsSlid WRITE setCapsSlid NOTIFY capsSlidChanged)
    Q_PROPERTY(cwPassageTubeSandboxModel* sandbox READ sandbox CONSTANT)
    Q_PROPERTY(bool sandboxSelected READ sandboxSelected NOTIFY currentIndexChanged)
    Q_PROPERTY(int firstCaseRow READ firstCaseRow CONSTANT)
    Q_PROPERTY(QString sandboxBuildText READ sandboxBuildText NOTIFY sandboxBuildChanged)

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

    // How far a ring's frame cross reaches from its center along right and up.
    static constexpr double kFrameAxisMeters = 0.5;

    static constexpr int kSandboxRow = 0;
    static constexpr int kSandboxDebounceMilliseconds = 16;
    static constexpr int kSandboxBuildBudgetMilliseconds = 100;
    static constexpr double kSandboxMaxSampleSpacingMeters = 4.0;

    // The rings of every chain as a line list. Each ring's loop runs through
    // its vertices back to the first; a grown ring draws every other edge.
    // rights and ups hold one segment per ring, in loop order, from its center
    // along frame.right and frame.up.
    //
    // readings hold a spoke per read wall of each station occurrence, from
    // the station as long as the reading: left and right square to its
    // facing, up and down along world z.
    //
    // sectionLoops hold, per ring with a section, the loop of its
    // stationRadii around the section's station in the ring's frame.
    // overshoots hold a whisker from the station's readings to each ring
    // vertex that cwPassageTubeMeshCheck reports as WallBeyondReading: more
    // than kWallTolerance past them.
    //
    // The ranges follow one another in the order declared.
    struct RingGeometry {
        QVector<QVector3D> points;
        QVector<cwLinePlotGeometry::VertexRange> loops;
        cwLinePlotGeometry::VertexRange rights;
        cwLinePlotGeometry::VertexRange ups;
        cwLinePlotGeometry::VertexRange readings;
        QVector<cwLinePlotGeometry::VertexRange> sectionLoops;
        cwLinePlotGeometry::VertexRange overshoots;
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

    bool readingsVisible() const;
    void setReadingsVisible(bool readingsVisible);

    // With a station(), the framing box also takes in the station's rings
    // and their sections.
    bool sectionsVisible() const;
    void setSectionsVisible(bool sectionsVisible);

    bool overshootVisible() const;
    void setOvershootVisible(bool overshootVisible);

    // The station the camera frames, matched ignoring case; empty, or a
    // station the current row lacks, frames the whole survey.
    QString station() const;
    void setStation(const QString& station);

    // Draws each chain's closed tube as it is, overlapping where chains meet,
    // in place of the builder's union, to compare the two. The list's issue
    // counts stay the builder's.
    bool rawTubesVisible() const;
    void setRawTubesVisible(bool rawTubesVisible);

    // Slides each chain's end cap into the passages it meets first
    // (cwPassageTubeBuilder::Options::slideCaps).
    bool capsSlid() const;
    void setCapsSlid(bool capsSlid);

    cwPassageTubeSandboxModel* sandbox() const;
    bool sandboxSelected() const;
    int firstCaseRow() const { return kSandboxRow + 1; }

    // The last sandbox build's time and sample spacing, and why the spacing
    // is coarser than the sandbox's; empty before the first build.
    QString sandboxBuildText() const;

    // True for a row built from a cwPassageTubeCases::JunctionCase, other
    // than the sandbox.
    Q_INVOKABLE bool canLoadSandbox(int row) const;

    // Copies the row's junction case into the sandbox and selects the
    // sandbox; false, leaving both alone, when canLoadSandbox(row) is false.
    Q_INVOKABLE bool loadSandbox(int row);

    // Adds cwPassageTubeCases::wideArmFork.
    Q_INVOKABLE void addWideArmFork();

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

    // The legs at station() and their readings, with the station's rings and
    // sections while sectionsVisible, else the mesh and centerline bounds, a
    // unit cube when the case is empty.
    QBox3D framingBox() const;

    // station(), else the first station where three or more passages meet,
    // the bounds center when there is none.
    QVector3D junction() const;

    // The far station of station()'s longest leg, else of the first arm into
    // junction(); and the horizontal unit direction from it toward junction().
    QVector3D armFarEnd() const;
    QVector3D armDirection() const;

    // The color the ring plot draws a layer in: "rings", "frames",
    // "readings", "sections" or "overshoot"; invalid for any other name.
    Q_INVOKABLE QColor layerColor(const QString& layer) const;

    cwPassageTubeMesh mesh() const;
    cwPassageTubeMeshCheck::Report report() const;
    RingGeometry ringGeometry() const;

    // Rings are numbered across chains in chain order. ringAt is the ring
    // whose center lies nearest point on screen, within maxPixels, else -1.
    Q_INVOKABLE int ringAt(QPointF point, const QMatrix4x4& modelToViewport, double maxPixels) const;
    Q_INVOKABLE int nearestRing(const QVector3D& position) const;

    // The ring's chain, index, leg, distance to its station, frame up,
    // inclination weight, radius range, kind, and its section's readings,
    // station and largest overshoot; empty for an unknown ring.
    Q_INVOKABLE QString ringLine(int ring) const;

signals:
    void countChanged();
    void includeSweepChanged();
    void currentIndexChanged();
    void sceneChanged();
    void currentChanged();
    void ringsVisibleChanged();
    void framesVisibleChanged();
    void readingsVisibleChanged();
    void sectionsVisibleChanged();
    void overshootVisibleChanged();
    void stationChanged();
    void rawTubesVisibleChanged();
    void capsSlidChanged();
    void sandboxBuildChanged();
    void projectOpened(int firstRow, int rowCount);
    void projectFailed(const QString& message);

private:
    // A catalog or random case, or one cave of an opened project;
    // junctionCase holds the case a junction row was made from.
    struct Row {
        QString name;
        QStringList tags;
        double sampleSpacing = 0.0;
        std::variant<cwPassageTubeCases::Case, cwPassageTubeCases::Survey> source;
        mutable std::optional<int> issueCount;
        mutable QString issueSummary;
        std::optional<cwPassageTubeCases::JunctionCase> junctionCase;

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
        // What the union did, for the report.
        QString unionText;
    };

    static Row caseRow(const cwPassageTubeCases::Case& entry);
    static Row caseRow(const cwPassageTubeCases::JunctionCase& junctionCase);
    static Build build(const cwPassageTubeCases::Survey& survey, double sampleSpacing, bool slideCaps);
    static Build buildRawTubes(const cwPassageTubeCases::Survey& survey, double sampleSpacing, bool slideCaps);
    static QString summary(const cwPassageTubeMeshCheck::Report& report);

    void loadCatalog();
    Row sandboxRow() const;
    double sandboxSampleSpacing() const;
    void sandboxEdited();
    void rebuildSandbox();
    void noteSandboxBuild(qint64 milliseconds, double sampleSpacing);
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
    bool m_readingsVisible = false;
    bool m_sectionsVisible = false;
    bool m_overshootVisible = false;
    QString m_station;
    bool m_rawTubesVisible = false;
    bool m_capsSlid = cwPassageTubeBuilder::Options().slideCaps;

    cwPassageTubeSandboxModel* m_sandbox = nullptr;
    QTimer* m_sandboxTimer = nullptr;
    int m_sandboxSpacingDoublings = 0;
    std::optional<qint64> m_sandboxBuildMilliseconds;
    double m_sandboxBuildSpacing = 0.0;

    Build m_current;
    RingGeometry m_ringGeometry;
    QBox3D m_bounds;
    QVector3D m_junction;
    QVector3D m_armFarEnd;
    std::optional<StationFrame> m_stationFrame;
};

#endif // CWPASSAGETUBEGALLERYMODEL_H
