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
#include "cwPassageTubeCases.h"
#include "cwPassageTubeMesh.h"
#include "cwPassageTubeMeshCheck.h"

//Qt includes
#include <QAbstractListModel>
#include <QBox3D>
#include <QPointer>
#include <QQmlEngine>
#include <QStringList>
#include <QVector3D>
#include <QVector>

//Std includes
#include <optional>

class cwRenderLinePlot;
class cwRenderPassageTube;
class cwScene;

/**
 * The tube gallery's list of surveys: the cwPassageTubeCases catalog plus
 * seeded random caves. Selecting a row builds it with
 * cwPassageTubeBuilder::buildDebug and checks it with
 * cwPassageTubeMeshCheck on the GUI thread; the mesh and centerline go to a
 * tube and a line plot the model adds to its scene.
 *
 * Issue counts for the list are built on first read and cached.
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

    // Adds a cwPassageTubeCases::random survey, or a randomWild one when wild.
    Q_INVOKABLE void addRandom(int seed, int junctionCount, bool wild = false);

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

    // The mesh and centerline bounds, a unit cube when the case is empty.
    QBox3D framingBox() const;

    // The first station where three or more passages meet, the bounds center
    // when there is none.
    QVector3D junction() const;

    // The far station of the first arm into junction(), and the horizontal unit
    // direction from it toward the junction.
    QVector3D armFarEnd() const;
    QVector3D armDirection() const;

    cwPassageTubeMesh mesh() const;
    cwPassageTubeMeshCheck::Report report() const;

signals:
    void countChanged();
    void includeSweepChanged();
    void currentIndexChanged();
    void sceneChanged();
    void currentChanged();

private:
    struct Row {
        cwPassageTubeCases::Case entry;
        mutable std::optional<int> issueCount;
        mutable QString issueSummary;
    };

    struct Build {
        cwPassageTubeBuilder::Debug debug;
        cwPassageTubeMeshCheck::Report report;
    };

    static Build build(const cwPassageTubeCases::Case& entry);
    static QString summary(const cwPassageTubeMeshCheck::Report& report);

    void loadCatalog();
    void cacheIssues(const Row& row, const cwPassageTubeMeshCheck::Report& report) const;
    void ensureIssueCount(const Row& row) const;
    void selectCurrent();
    void locateJunction(const cwPassageTubeCases::Trip& trip);
    void pushToScene();
    void deleteRenderObjects();

    QVector<Row> m_rows;
    QVector<cwPassageTubeCases::Case> m_randomCases;
    bool m_includeSweep = false;
    int m_currentIndex = -1;

    QPointer<cwScene> m_scene;
    QPointer<cwRenderPassageTube> m_tube;
    QPointer<cwRenderLinePlot> m_linePlot;

    Build m_current;
    QBox3D m_bounds;
    QVector3D m_junction;
    QVector3D m_armFarEnd;
    QVector3D m_armDirection;
};

#endif // CWPASSAGETUBEGALLERYMODEL_H
