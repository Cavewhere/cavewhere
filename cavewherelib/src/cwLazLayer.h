/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWLAZLAYER_H
#define CWLAZLAYER_H

//Qt includes
#include <QDateTime>
#include <QObject>
#include <QQmlEngine>
#include <QString>
#include <QUuid>
#include <QVector3D>
#include <QtTypes>

//AsyncFuture
#include <asyncfuture.h>

//Our includes
#include "CaveWhereLibExport.h"
#include "cwFutureManagerToken.h"
#include "cwGeometry.h"
#include "cwGeoPoint.h"
#include "cwLazLoader.h"

class cwKeywordModel;

/**
 * Single LAZ point-cloud layer.
 *
 * Owns the loaded cwGeometry plus async load state. sourcePath is the only
 * persisted field; everything else (bbox, source CS, point count, geometry)
 * is rebuilt by re-running cwLazLoader against the file.
 *
 * Visibility is not a property here — it lives in the render system via
 * cwKeywordItemModel + a per-layer render-side shim, the same architecture
 * cwScrap uses.
 */
class CAVEWHERE_LIB_EXPORT cwLazLayer : public QObject
{
    Q_OBJECT
    QML_NAMED_ELEMENT(LazLayer)
    QML_UNCREATABLE("Created via cwLazLayerModel::addLayer")

    Q_PROPERTY(QString sourcePath READ sourcePath WRITE setSourcePath NOTIFY sourcePathChanged)
    Q_PROPERTY(double pointSize READ pointSize WRITE setPointSize NOTIFY pointSizeChanged)

    Q_PROPERTY(QString name READ name NOTIFY nameChanged)
    Q_PROPERTY(QString sourceCS READ sourceCS NOTIFY sourceCSChanged)
    Q_PROPERTY(LoadStatus loadStatus READ loadStatus NOTIFY loadStatusChanged)
    Q_PROPERTY(QString errorMessage READ errorMessage NOTIFY errorMessageChanged)
    Q_PROPERTY(int pointCount READ pointCount NOTIFY pointCountChanged)
    Q_PROPERTY(QVector3D bboxMin READ bboxMin NOTIFY bboxChanged)
    Q_PROPERTY(QVector3D bboxMax READ bboxMax NOTIFY bboxChanged)
    Q_PROPERTY(float meanSpacingXY READ meanSpacingXY NOTIFY meanSpacingXYChanged)
    Q_PROPERTY(cwKeywordModel* keywordModel READ keywordModel CONSTANT)

public:
    enum class LoadStatus {
        Idle,
        Loading,
        Loaded,
        Error
    };
    Q_ENUM(LoadStatus)

    explicit cwLazLayer(QObject* parent = nullptr);
    ~cwLazLayer() override;

    QString sourcePath() const { return m_sourcePath; }
    void setSourcePath(const QString& path);

    /// File size and last-modified timestamp captured the last time
    /// setSourcePath ran. cwLazLayerModel::rescan uses the pair to detect
    /// in-place file overwrites and force a reload.
    qint64 sourceSize() const { return m_sourceSize; }
    QDateTime sourceMtime() const { return m_sourceMtime; }

    double pointSize() const { return m_pointSize; }
    void setPointSize(double pointSize);

    QString name() const { return m_name; }
    QString sourceCS() const { return m_sourceCS; }
    QString sourceCSOverride() const { return m_sourceCSOverride; }
    void setSourceCSOverride(const QString& cs);

    LoadStatus loadStatus() const { return m_loadStatus; }
    QString errorMessage() const { return m_errorMessage; }
    int pointCount() const { return m_geometry.vertexCount(); }
    QVector3D bboxMin() const { return m_bboxMin; }
    QVector3D bboxMax() const { return m_bboxMax; }
    float meanSpacingXY() const { return m_meanSpacingXY; }

    QUuid id() const { return m_id; }

    const cwGeometry& geometry() const { return m_geometry; }
    cwKeywordModel* keywordModel() const { return m_keywordModel; }

    void setFutureManagerToken(const cwFutureManagerToken& token);
    void setRegionGlobalCS(const QString& cs);
    void setRegionWorldOrigin(const cwGeoPoint& origin);

    /// Kicks off an async load against the current globalCS / worldOrigin.
    /// Re-entrant: rapid restarts coalesce into a single in-flight run.
    void reload();

signals:
    void sourcePathChanged();
    void pointSizeChanged();
    void nameChanged();
    void sourceCSChanged();
    void loadStatusChanged();
    void errorMessageChanged();
    void pointCountChanged();
    void bboxChanged();
    void meanSpacingXYChanged();

private:
    void setLoadStatus(LoadStatus status);
    void setErrorMessage(const QString& message);
    void updateNameKeyword();
    void updateFileNameKeyword();
    void updateIdKeyword();
    void updateTypeKeyword();
    void applyResult(cwLazLoadResult&& result);

    QString m_sourcePath;
    qint64 m_sourceSize = -1;
    QDateTime m_sourceMtime;
    double m_pointSize = 2.0;

    QString m_name;
    QString m_sourceCS; // CS used during the last load (override > LAZ-embedded > "")
    QString m_sourceCSOverride;
    LoadStatus m_loadStatus = LoadStatus::Idle;
    QString m_errorMessage;
    QVector3D m_bboxMin;
    QVector3D m_bboxMax;
    float m_meanSpacingXY = 0.0f;
    QUuid m_id;

    cwGeometry m_geometry;

    cwKeywordModel* m_keywordModel = nullptr;
    cwFutureManagerToken m_futureManagerToken;
    QString m_regionGlobalCS;
    cwGeoPoint m_regionWorldOrigin;

    // Coalesces rapid reload() calls and serializes cancel-then-restart so a
    // new load only begins once the previous one has actually stopped.
    AsyncFuture::Restarter<cwLazLoadResult> m_loadRestarter;
};

#endif // CWLAZLAYER_H
