/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWLAZLAYERMODEL_H
#define CWLAZLAYERMODEL_H

//Qt includes
#include <QAbstractListModel>
#include <QDir>
#include <QList>
#include <QQmlEngine>
#include <QSet>
#include <QUrl>

//Our includes
#include "CaveWhereLibExport.h"
#include "cwFutureManagerToken.h"
#include "cwLazLayer.h"
#include "cwLazLayerData.h"

class cwCavingRegion;
class cwProject;

class CAVEWHERE_LIB_EXPORT cwLazLayerModel : public QAbstractListModel
{
    Q_OBJECT
    QML_NAMED_ELEMENT(LazLayerModel)
    QML_UNCREATABLE("Owned by cwCavingRegion")

    Q_PROPERTY(int count READ count NOTIFY countChanged)

public:
    enum Roles {
        LayerRole = Qt::UserRole + 1,
        NameRole,
        SourcePathRole,
        SourceCSRole,
        SourceCSDisplayNameRole,
        PointSizeRole,
        LoadStatusRole,
        PointCountRole,
        EnabledRole
    };
    Q_ENUM(Roles)

    explicit cwLazLayerModel(QObject* parent = nullptr);
    ~cwLazLayerModel() override;

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    bool setData(const QModelIndex& index, const QVariant& value, int role) override;
    QHash<int, QByteArray> roleNames() const override;

    Q_INVOKABLE void addFromFiles(QList<QUrl> urls);
    Q_INVOKABLE void removeAt(int index);
    Q_INVOKABLE cwLazLayer* layerAt(int index) const;
    Q_INVOKABLE void rescan();

    /// Rename the layer at `row` to `newBasename`. The new basename is
    /// the bare filename without extension (validation rejects empty
    /// strings, path separators, and strings containing a dot). On
    /// success, the layer's source path is updated in-memory
    /// immediately, the layerRenamed signal fires so cwSaveLoad can
    /// queue the paired .laz + .cwlaz Move jobs, and dataChanged is
    /// emitted for the row. On failure the model is untouched and a
    /// user-visible cwError is appended to the project's errorModel
    /// (same surface rescan() uses to report failed metadata loads).
    Q_INVOKABLE bool rename(int row, const QString& newBasename);

    int count() const { return m_layers.size(); }
    const QList<cwLazLayer*>& layers() const { return m_layers; }

    void setFutureManagerToken(const cwFutureManagerToken& token);
    cwFutureManagerToken futureManagerToken() const { return m_futureManagerToken; }

    //! Hand every layer, and every layer made later, where to get the frame it
    //! decodes into.
    void setLocalProjectionToken(const cwLocalProjectionToken& token);

    //! Re-decode every layer. The region calls this when the frame moves: the
    //! points in memory are in the frame they were loaded into, and each layer
    //! reads the new one back through its token.
    void reloadAll();

    //! Whether any layer is still reading its header. The local projection
    //! derives the frame from what those headers say, so it waits for all of
    //! them rather than anchoring on whichever landed first. Counted from the
    //! moment a layer is created, which is before it is announced as a row: the
    //! read starts there too.
    bool anyHeaderProbeInFlight() const { return !m_probingLayers.isEmpty(); }

    void setGisLayersDir(const QDir& dir);
    QDir gisLayersDir() const { return m_gisLayersDir; }

    void clear();

    static QString folderName();

signals:
    void countChanged();

    /// Emitted when the model goes from no layer reading a header to one, and
    /// back again — the two edges the local projection cares about, rather than
    /// one signal per file of a directory being scanned.
    void headerProbeInFlightChanged();

    /// Emitted only from removeAt() — i.e. when the user explicitly removes
    /// a layer through the UI. cwSaveLoad listens for this to enqueue the
    /// paired .laz + .cwlaz deletions. Rescan-driven removals (file
    /// vanished from disk by other means) deliberately do NOT emit this,
    /// so the paired .cwlaz survives the in-memory removal and identity
    /// can resume if the .laz reappears later.
    void aboutToRemoveLayerByUser(cwLazLayer* layer);

    /// Emitted from rename() after the layer's in-memory source path has
    /// been updated but before any filesystem work has been queued. Past
    /// tense matches the actual emit ordering — at signal-emit time
    /// `layer->sourcePath()` already returns the new path. cwSaveLoad
    /// listens to enqueue the .laz Move (explicit-path, tag "source")
    /// and the .cwlaz Move (via the m_objectStates-tracked path, empty
    /// tag). The two paths are passed in the signal because oldSourcePath
    /// is otherwise unrecoverable from a synchronous handler — without
    /// it the .laz Move can't be constructed.
    void layerRenamed(cwLazLayer* layer,
                      const QString& oldSourcePath,
                      const QString& newSourcePath);

private:
    void connectLayer(cwLazLayer* layer);
    int indexOf(cwLazLayer* layer) const;
    cwProject* project() const;
    cwLazLayer* createLayer();
    //! Track \a layer among the layers still reading a header. Keyed on the
    //! layer rather than counted, so that a probe restarted before its result
    //! arrives can't be double-counted and a destroyed layer can't leave one
    //! behind.
    void updateHeaderProbeInFlight(cwLazLayer* layer, bool inFlight);

    QList<cwLazLayer*> m_layers;
    QSet<cwLazLayer*> m_probingLayers;
    cwFutureManagerToken m_futureManagerToken;
    cwLocalProjectionToken m_localProjectionToken;
    QDir m_gisLayersDir;
};

#endif // CWLAZLAYERMODEL_H
