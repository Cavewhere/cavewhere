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

    static QString folderName();

signals:
    void countChanged();

private:
    void connectLayer(cwLazLayer* layer);
    int indexOf(cwLazLayer* layer) const;
    void maybeAdoptRegionDefaultsFromLaz(const QString& sourcePath);
    cwProject* project() const;
    cwLazLayer* createLayer();

    QList<cwLazLayer*> m_layers;
    cwFutureManagerToken m_futureManagerToken;
    QString m_regionGlobalCS;
    cwGeoPoint m_regionWorldOrigin;
    QDir m_gisLayersDir;
};

#endif // CWLAZLAYERMODEL_H
