#ifndef CWCAVEPAGEMODEL_H
#define CWCAVEPAGEMODEL_H

#include <QAbstractItemModel>
#include <QPointer>
#include <QQmlEngine>

#include "cwCave.h"
#include "CaveWhereLibExport.h"
class cwTrip;
class cwTripLengthTask;
class cwUsedStationTaskManager;

class CAVEWHERE_LIB_EXPORT cwCavePageModel : public QAbstractItemModel
{
    Q_OBJECT
    QML_NAMED_ELEMENT(CavePageModel)

    Q_PROPERTY(cwCave* cave READ cave WRITE setCave NOTIFY caveChanged)

public:
    enum Roles {
        TripObjectRole = Qt::UserRole + 1,
        ErrorCountRole,
        TripNameRole,
        TripDateRole,
        UsedStationsRole,
        TripDistanceRole
    };
    Q_ENUM(Roles);

    explicit cwCavePageModel(QObject *parent = nullptr);
    ~cwCavePageModel();

    // QAbstractItemModel interface
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QModelIndex index(int row, int column = 0, const QModelIndex &parent = QModelIndex()) const override;
    QModelIndex parent(const QModelIndex &child) const override;
    int columnCount(const QModelIndex &parent = QModelIndex()) const override;
    QHash<int, QByteArray> roleNames() const override;

    cwCave* cave() const;
    void setCave(cwCave* cave);

    void waitForFinished();

signals:
    void caveChanged();

private:
    QPointer<cwCave> m_cave;

    struct TripData {
        QPointer<cwTrip> trip;
        cwTripLengthTask* lengthTask;
        cwUsedStationTaskManager* usedStationsManager;

        qreal length = 0.0;
        QStringList usedStations;
    };

    QVector<TripData> m_tripDataList;
};

#endif // CWCAVEPAGEMODEL_H
