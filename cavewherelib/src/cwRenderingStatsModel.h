/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWRENDERINGSTATSMODEL_H
#define CWRENDERINGSTATSMODEL_H

//Qt includes
#include <QAbstractListModel>
#include <QList>
#include <QQmlEngine>
#include <QString>
#include <QTimer>

//Our includes
#include "cwGlobals.h"
#include "cwRenderCullingStats.h"
#include "cwTextureStreamingStats.h"

// Read-only view of cwRenderMemoryLedger for QML. One row per ledger category,
// refreshed by a timer while running is true.
class CAVEWHERE_LIB_EXPORT cwRenderingStatsModel : public QAbstractListModel
{
    Q_OBJECT
    QML_NAMED_ELEMENT(RenderingStatsModel)
    Q_PROPERTY(qint64 totalGpuBytes READ totalGpuBytes NOTIFY totalsChanged)
    Q_PROPERTY(qint64 totalCpuBytes READ totalCpuBytes NOTIFY totalsChanged)
    Q_PROPERTY(QString totalGpuText READ totalGpuText NOTIFY totalsChanged)
    Q_PROPERTY(QString totalCpuText READ totalCpuText NOTIFY totalsChanged)
    Q_PROPERTY(bool running READ running WRITE setRunning NOTIFY runningChanged)
    Q_PROPERTY(int totalObjects READ totalObjects NOTIFY cullingChanged)
    Q_PROPERTY(int culledObjects READ culledObjects NOTIFY cullingChanged)
    Q_PROPERTY(int totalItems READ totalItems NOTIFY cullingChanged)
    Q_PROPERTY(int culledItems READ culledItems NOTIFY cullingChanged)
    Q_PROPERTY(int streamedItems READ streamedItems NOTIFY streamingChanged)
    Q_PROPERTY(int loadsInFlight READ loadsInFlight NOTIFY streamingChanged)
    Q_PROPERTY(int itemsBelowDesired READ itemsBelowDesired NOTIFY streamingChanged)
    Q_PROPERTY(qint64 readyCpuBytes READ readyCpuBytes NOTIFY streamingChanged)
    Q_PROPERTY(QString readyCpuText READ readyCpuText NOTIFY streamingChanged)
    Q_PROPERTY(int demotionsInFlight READ demotionsInFlight NOTIFY streamingChanged)

public:
    enum Roles {
        NameRole = Qt::UserRole,
        GpuBytesRole,
        CpuBytesRole,
        GpuTextRole,
        CpuTextRole
    };

    explicit cwRenderingStatsModel(QObject* parent = nullptr);

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    bool running() const { return m_running; }
    void setRunning(bool running);

    qint64 totalGpuBytes() const { return m_totalGpuBytes; }
    qint64 totalCpuBytes() const { return m_totalCpuBytes; }
    QString totalGpuText() const { return formattedBytes(m_totalGpuBytes); }
    QString totalCpuText() const { return formattedBytes(m_totalCpuBytes); }

    int totalObjects() const { return m_culling.objectsTotal; }
    int culledObjects() const { return m_culling.objectsCulled; }
    int totalItems() const { return m_culling.itemsTotal; }
    int culledItems() const { return m_culling.itemsCulled; }

    int streamedItems() const { return m_streaming.streamedItems; }
    int loadsInFlight() const { return m_streaming.loadsInFlight; }
    int itemsBelowDesired() const { return m_streaming.itemsBelowDesired; }
    qint64 readyCpuBytes() const { return m_streaming.readyCpuBytes; }
    QString readyCpuText() const { return formattedBytes(m_streaming.readyCpuBytes); }
    int demotionsInFlight() const { return m_streaming.demotionsInFlight; }

    //! Re-reads the ledger now, for the HUD's refresh affordance
    Q_INVOKABLE void refresh();

    //! Bytes as B/KB/MB/GB, base 1024, one decimal above a kilobyte
    static QString formattedBytes(qint64 bytes);

signals:
    void totalsChanged();
    void runningChanged();
    void cullingChanged();
    void streamingChanged();

private slots:
    void poll();

private:
    struct Row {
        qint64 gpuBytes = 0;
        qint64 cpuBytes = 0;
    };

    QTimer m_timer;
    QList<Row> m_rows;
    qint64 m_totalGpuBytes = 0;
    qint64 m_totalCpuBytes = 0;
    quint64 m_lastRevision = 0;
    cwRenderCullingStats::Counts m_culling;
    quint64 m_lastCullingRevision = 0;
    cwTextureStreamingStats::Counts m_streaming;
    quint64 m_lastStreamingRevision = 0;
    bool m_running = false;
};

#endif // CWRENDERINGSTATSMODEL_H
