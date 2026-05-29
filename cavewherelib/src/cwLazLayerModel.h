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
#include <QUrl>

//Our includes
#include "CaveWhereLibExport.h"
#include "cwFutureManagerToken.h"
#include "cwGeoPoint.h"
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
        PointSizeRole,
        LoadStatusRole,
        PointCountRole
    };
    Q_ENUM(Roles)

    explicit cwLazLayerModel(QObject* parent = nullptr);
    ~cwLazLayerModel() override;

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    Q_INVOKABLE void addFromFiles(QList<QUrl> urls);
    Q_INVOKABLE void removeAt(int index);
    Q_INVOKABLE cwLazLayer* layerAt(int index) const;
    Q_INVOKABLE void rescan();

    int count() const { return m_layers.size(); }
    const QList<cwLazLayer*>& layers() const { return m_layers; }

    void setFutureManagerToken(const cwFutureManagerToken& token);
    cwFutureManagerToken futureManagerToken() const { return m_futureManagerToken; }
    void setRegionGlobalCS(const QString& cs);
    void setRegionWorldOrigin(const cwGeoPoint& origin);

    void setGisLayersDir(const QDir& dir);
    QDir gisLayersDir() const { return m_gisLayersDir; }

    void clear();

    /// Snapshot persistable state for every layer the model knows about,
    /// merged with carried-over entries for layers that were persisted in a
    /// previous session but aren't currently present (e.g. during a saveAs
    /// transient when the model has been cleared and is being re-populated).
    QList<cwLazLayerData> data() const;

    /// Apply persisted state. Entries that match an existing layer (keyed by
    /// fileName basename) are applied immediately; entries that don't match
    /// are stored as a pending overlay and applied as matching layers are
    /// later inserted by rescan(). Pending entries survive clear(), so a
    /// saveAs transient doesn't lose state; they're pruned only when the
    /// user removes a layer via removeAt().
    void setData(const QList<cwLazLayerData>& data);

    static QString folderName();

signals:
    void countChanged();

private:
    void connectLayer(cwLazLayer* layer);
    int indexOf(cwLazLayer* layer) const;
    void maybeAdoptRegionDefaultsFromLaz(const QString& sourcePath);
    cwProject* project() const;
    cwLazLayer* createLayer();
    void applyPendingStateTo(cwLazLayer* layer);

    QList<cwLazLayer*> m_layers;
    cwFutureManagerToken m_futureManagerToken;
    QString m_regionGlobalCS;
    cwGeoPoint m_regionWorldOrigin;
    QDir m_gisLayersDir;

    // Per-layer state that has been handed to setData() but hasn't yet been
    // applied to a matching layer — usually because the layer hasn't been
    // discovered by rescan() yet. Survives clear() so saveAs transients
    // don't drop state. Pruned by removeAt() so a user-removed layer
    // doesn't silently re-disable its same-named successor.
    QList<cwLazLayerData> m_pendingStates;
};

#endif // CWLAZLAYERMODEL_H
