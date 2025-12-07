#ifndef CWNOTELIDAR_H
#define CWNOTELIDAR_H

// Qt includes
#include <QObject>
#include <QList>
#include <QUrl>
#include <QString>
#include <QtQml/qqmlregistration.h>
#include <QAbstractListModel>
#include <QProperty>
#include <QMatrix4x4>
#include <QQuaternion>

// Our includes
#include "cwGlobals.h"
#include "cwNoteLiDARStation.h"
#include "cwNoteLiDARData.h"
#include "cwNoteLiDARTransformation.h"
#include "cwKeywordModel.h"
class cwTrip;
class cwCave;

class CAVEWHERE_LIB_EXPORT cwNoteLiDAR : public QAbstractListModel
{
    Q_OBJECT
    QML_NAMED_ELEMENT(NoteLiDAR)

    Q_PROPERTY(QString name READ name WRITE setName NOTIFY nameChanged BINDABLE bindableName)

    Q_PROPERTY(QString filename READ filename WRITE setFilename NOTIFY filenameChanged)
    Q_PROPERTY(int count READ rowCount NOTIFY countChanged)

    Q_PROPERTY(cwNoteLiDARTransformation* noteTransformation READ noteTransformation NOTIFY noteTransformationChanged FINAL)
    Q_PROPERTY(bool autoCalculateNorth READ autoCalculateNorth WRITE setAutoCalculateNorth NOTIFY autoCalculateNorthChanged BINDABLE bindableAutoCalculateNorth)

    Q_PROPERTY(QString iconImagePath READ iconImagePath WRITE setIconImagePath NOTIFY iconImagePathChanged BINDABLE bindableIconImagePath)
    Q_PROPERTY(cwKeywordModel* keywordModel READ keywordModel CONSTANT)


    // Q_PROPERTY(QMatrix4x4 modelMatrix READ modelMatrix WRITE setModelMatrix NOTIFY modelMatrixChanged BINDABLE bindableModelMatrix)

public:
    enum Role {
        NameRole = Qt::UserRole + 1,
        PositionOnNoteRole,
        ScenePositionRole,
        UpPositionRole,
        StationRole
    };
    Q_ENUM(Role)

    explicit cwNoteLiDAR(QObject* parent = nullptr);

    // QAbstractListModel interface
    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    bool setData(const QModelIndex &index, const QVariant &value, int role) override;

    QHash<int, QByteArray> roleNames() const override;

    // Property accessors
    QString name() const { return m_name.value(); }
    void setName(const QString& name) { m_name = name; }
    QBindable<QString> bindableName() { return &m_name; }

    QString filename() const;
    void setFilename(const QString& filename);

    void setParentTrip(cwTrip* trip);
    Q_INVOKABLE cwTrip* parentTrip() const { return m_parentTrip; }
    cwKeywordModel* keywordModel() const { return m_keywordModel; }

    cwCave* parentCave() const;

    // Stations API
    void addStation(const cwNoteLiDARStation& station);
    Q_INVOKABLE void removeStation(int stationId);         // removes by index
    const QList<cwNoteLiDARStation>& stations() const;
    void setStations(const QList<cwNoteLiDARStation>& stations);
    Q_INVOKABLE cwNoteLiDARStation station(int stationId) const;

    QString iconImagePath() const { return m_iconImagePath.value(); }
    void setIconImagePath(const QString& path) { m_iconImagePath = path; }
    QBindable<QString> bindableIconImagePath() { return &m_iconImagePath; }

    cwNoteLiDARData data() const;
    void setData(const cwNoteLiDARData& data);

    cwNoteLiDARTransformation *noteTransformation() const;

    bool autoCalculateNorth() const { return m_autoCalculateNorth.value(); }
    void setAutoCalculateNorth(const bool& autoCalculateNorth) { m_autoCalculateNorth = autoCalculateNorth; }
    QBindable<bool> bindableAutoCalculateNorth() { return &m_autoCalculateNorth; }

signals:
    void nameChanged();

    void filenameChanged();
    void countChanged();
    void modelMatrixChanged();
    void upRotationChanged();

    void noteTransformationChanged();
    void autoCalculateNorthChanged();
    void iconImagePathChanged();

private:
    QString m_filename;                         // glTF binary file path (.glb)
    QList<cwNoteLiDARStation> m_stations;       // stations associated with this LiDAR note

    cwTrip* m_parentTrip = nullptr;
    cwNoteLiDARTransformation *m_noteTransformation = nullptr;
    cwKeywordModel* m_keywordModel = nullptr;

    Q_OBJECT_BINDABLE_PROPERTY_WITH_ARGS(cwNoteLiDAR, QString, m_name, QString(), &cwNoteLiDAR::nameChanged);
    Q_OBJECT_BINDABLE_PROPERTY_WITH_ARGS(cwNoteLiDAR, bool, m_autoCalculateNorth, true, &cwNoteLiDAR::autoCalculateNorthChanged);
    Q_OBJECT_BINDABLE_PROPERTY_WITH_ARGS(cwNoteLiDAR, QString, m_iconImagePath, QString(), &cwNoteLiDAR::iconImagePathChanged);
    // Q_OBJECT_BINDABLE_PROPERTY_WITH_ARGS(cwNoteLiDAR, QMatrix4x4, m_modelMatrix, QMatrix4x4(), &cwNoteLiDAR::modelMatrixChanged);

    int clampIndex(int stationId) const;

    void updateNoteTransformion();

};


#endif // CWNOTELIDAR_H
