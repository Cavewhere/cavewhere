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
#include <QList>
#include <QQmlEngine>

//Our includes
#include "CaveWhereLibExport.h"
#include "cwFutureManagerToken.h"
#include "cwGeoPoint.h"
#include "cwLazLayer.h"

//Protobuf
namespace CavewhereProto {
class ProjectMetadata;
}

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
        LoadProgressRole,
        PointCountRole
    };
    Q_ENUM(Roles)

    explicit cwLazLayerModel(QObject* parent = nullptr);
    ~cwLazLayerModel() override;

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    Q_INVOKABLE cwLazLayer* addLayer(const QString& sourcePath);
    Q_INVOKABLE void removeAt(int index);
    Q_INVOKABLE cwLazLayer* layerAt(int index) const;

    int count() const { return m_layers.size(); }
    const QList<cwLazLayer*>& layers() const { return m_layers; }

    void setFutureManagerToken(const cwFutureManagerToken& token);
    void setRegionGlobalCS(const QString& cs);
    void setRegionWorldOrigin(const cwGeoPoint& origin);

    void writeTo(CavewhereProto::ProjectMetadata* metadata) const;
    void readFrom(const CavewhereProto::ProjectMetadata& metadata);

    void clear();

signals:
    void countChanged();

private:
    void connectLayer(cwLazLayer* layer);
    int indexOf(cwLazLayer* layer) const;

    QList<cwLazLayer*> m_layers;
    cwFutureManagerToken m_futureManagerToken;
    QString m_regionGlobalCS;
    cwGeoPoint m_regionWorldOrigin;
};

#endif // CWLAZLAYERMODEL_H
