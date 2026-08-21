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
#include "cwLazLayerData.h"
#include "cwLazLoader.h"
#include "cwLocalProjectionToken.h"

//Std includes
#include <optional>

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
    Q_PROPERTY(bool enabled READ enabled WRITE setEnabled NOTIFY enabledChanged)

    Q_PROPERTY(QString name READ name NOTIFY nameChanged)
    Q_PROPERTY(QString sourceCS READ sourceCS NOTIFY sourceCSChanged)
    Q_PROPERTY(QString sourceCSDisplayName READ sourceCSDisplayName NOTIFY sourceCSChanged)
    Q_PROPERTY(LoadStatus loadStatus READ loadStatus NOTIFY loadStatusChanged)
    Q_PROPERTY(QString errorMessage READ errorMessage NOTIFY errorMessageChanged)
    Q_PROPERTY(qint64 pointCount READ pointCount NOTIFY pointCountChanged)
    Q_PROPERTY(QVector3D bboxMin READ bboxMin NOTIFY bboxChanged)
    Q_PROPERTY(QVector3D bboxMax READ bboxMax NOTIFY bboxChanged)
    Q_PROPERTY(float meanSpacingXY READ meanSpacingXY NOTIFY meanSpacingXYChanged)
    Q_PROPERTY(cwKeywordModel* keywordModel READ keywordModel CONSTANT)

public:
    enum class LoadStatus {
        Idle,
        //! The header has been read but the points have not — where a disabled
        //! layer rests, and where an enabled one passes through on its way to
        //! Loading. The file's own CRS and bounding box are already known here.
        Probed,
        Loading,
        Loaded,
        Error
    };
    Q_ENUM(LoadStatus)

    explicit cwLazLayer(QObject* parent = nullptr);
    ~cwLazLayer() override;

    QString sourcePath() const { return m_sourcePath; }
    void setSourcePath(const QString& path);

    /// Update the source path WITHOUT re-running the async loader. Used by
    /// cwLazLayerModel::rename when the file is being moved on disk to a
    /// sibling basename inside the same directory — the bytes are unchanged,
    /// so the already-loaded geometry stays valid. setSourcePath would
    /// trigger a reload that races against the queued Move job (the new path
    /// doesn't exist on disk yet when the rename is enqueued). Updates
    /// m_sourcePath, the cached fingerprint (size/mtime), m_name, and fires
    /// sourcePathChanged + nameChanged accordingly.
    void renameSourcePath(const QString& newPath);

    /// File size and last-modified timestamp captured the last time
    /// setSourcePath ran. cwLazLayerModel::rescan uses the pair to detect
    /// in-place file overwrites and force a reload.
    qint64 sourceSize() const { return m_sourceSize; }
    QDateTime sourceMtime() const { return m_sourceMtime; }

    double pointSize() const { return m_pointSize; }
    void setPointSize(double pointSize);

    bool enabled() const { return m_enabled; }
    void setEnabled(bool enabled);

    QString name() const { return m_name; }

    /// The CRS the file is read as: the override if one is set, otherwise what
    /// the file's own header declares, otherwise empty (identity).
    QString sourceCS() const;
    QString sourceCSDisplayName() const;
    QString sourceCSOverride() const { return m_sourceCSOverride; }
    void setSourceCSOverride(const QString& cs);

    /// The bounding box in the file's own CRS — the raw header numbers, before
    /// the reprojection into the region's frame that bboxMin and bboxMax carry.
    /// Filled by the header probe, so they arrive well before the points do.
    /// Deriving a coordinate frame from this cloud needs these.
    cwGeoPoint sourceBboxMin() const;
    cwGeoPoint sourceBboxMax() const;
    cwGeoPoint sourceBboxCenter() const;

    /// True once this file's header has been read, which is the point at which
    /// sourceCS and the source bounding box carry the file's own numbers. It
    /// says nothing about the points: a disabled layer decodes none and still
    /// knows where it is. That is what lets the local projection treat every
    /// layer as a georeferenced input, and so tell an absent one from a
    /// merely-unfinished one.
    bool hasReadHeader() const { return m_header.has_value(); }

    /// True from the moment a header read is asked for until its result has
    /// been applied. The local projection is derived from what the headers say,
    /// so this is what tells it that the set of layers it can see is still
    /// growing — see cwLocalProjectionManager::frameFuture().
    bool headerProbeInFlight() const { return m_headerProbeInFlight; }

    /// Where the layer stands, as one value for the UI to show. It combines the
    /// two things that happen independently — the header read and the point
    /// decode — so Probed is not a state anything stores: it is what "the
    /// header is in, the points are not" is called.
    LoadStatus loadStatus() const;
    QString errorMessage() const { return m_errorMessage; }
    qint64 pointCount() const { return m_geometry.vertexCount(); }
    QVector3D bboxMin() const { return m_bboxMin; }
    QVector3D bboxMax() const { return m_bboxMax; }
    float meanSpacingXY() const { return m_meanSpacingXY; }

    QUuid id() const { return m_id; }

    const cwGeometry& geometry() const { return m_geometry; }
    cwKeywordModel* keywordModel() const { return m_keywordModel; }

    void setFutureManagerToken(const cwFutureManagerToken& token);

    /// Where this layer gets the frame its points are loaded into, and when
    /// that frame is final. Every reload asks it again.
    void setLocalProjectionToken(const cwLocalProjectionToken& token);

    /// Kicks off an async load into the frame the token names. Call it after
    /// the frame moves: the geometry in memory is in the old one.
    /// Re-entrant: rapid restarts coalesce into a single in-flight run.
    void reload();

    /// Snapshot the persistable per-layer state. fileName is derived from
    /// sourcePath (the LAZ file's basename) and is the key cwLazLayerModel
    /// uses to match a state entry to a layer.
    cwLazLayerData data() const;

    /// Apply persisted per-layer state. Only the enabled bit is applied;
    /// fileName is treated as identity and matching is the model's job.
    void setData(const cwLazLayerData& data);

signals:
    void sourcePathChanged();
    void pointSizeChanged();
    void enabledChanged();
    void nameChanged();
    void sourceCSChanged();
    void loadStatusChanged();
    void errorMessageChanged();
    void pointCountChanged();
    void bboxChanged();
    void meanSpacingXYChanged();
    void headerProbeInFlightChanged();

private:
    //! How far the points have got, which is all this layer actually runs.
    //! loadStatus() folds it together with whether the header is in.
    enum class PointState {
        Idle,
        Loading,
        Loaded,
        Error
    };

    void setPointState(PointState state);
    void setErrorMessage(const QString& message);
    void updateNameKeyword();
    void updateFileNameKeyword();
    void updateIdKeyword();
    void updateTypeKeyword();
    void applyResult(cwLazLoadResult&& result);
    void startHeaderProbe();
    //! Publish what @a probe read, then report the probe finished — in that
    //! order, which is the whole reason this is one function.
    void finishProbe(const cwLazLoader::ProbeResult& probe);
    void setHeaderProbeInFlight(bool inFlight);
    void startDecode(const QString& frameCS);

    QString m_sourcePath;
    qint64 m_sourceSize = -1;
    QDateTime m_sourceMtime;
    double m_pointSize = 2.0;
    bool m_enabled = true;

    QString m_name;
    QString m_sourceCSOverride;
    QString m_errorMessage;
    QVector3D m_bboxMin;
    QVector3D m_bboxMax;

    // What the file says about itself, kept whole rather than shredded into
    // fields plus a bool saying they mean anything. Empty until the header has
    // been read; the override is applied on the way out, in sourceCS().
    std::optional<cwLazLoader::ProbeResult> m_header;

    PointState m_pointState = PointState::Idle;
    bool m_headerProbeInFlight = false;
    float m_meanSpacingXY = 0.0f;
    QUuid m_id;

    cwGeometry m_geometry;

    cwKeywordModel* m_keywordModel = nullptr;
    cwFutureManagerToken m_futureManagerToken;
    cwLocalProjectionToken m_localProjectionToken;

    // Which wait for the frame is the current one. A reload() that starts while
    // an earlier one is still waiting supersedes it: the newer one carries the
    // newer frame, and both would otherwise decode the same file.
    quint64 m_frameWaitGeneration = 0;

    // Coalesces rapid reload() calls and serializes cancel-then-restart so a
    // new load only begins once the previous one has actually stopped.
    AsyncFuture::Restarter<cwLazLoadResult> m_loadRestarter;

    // Its own restarter so probe results arrive in the order they were asked
    // for: two probes racing on a file rewritten twice could otherwise publish
    // the older header last.
    AsyncFuture::Restarter<cwLazLoader::ProbeResult> m_probeRestarter;
};

#endif // CWLAZLAYER_H
