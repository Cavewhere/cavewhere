/**************************************************************************
**
**    Copyright (C) 2025
**
**************************************************************************/

#ifndef CWITEM3DREPEATER_H
#define CWITEM3DREPEATER_H

// Qt includes
#include <QPointer>
#include <QAbstractItemModel>
#include <QVector3D>

// Our includes
#include "cwAbstractPointManager.h"
#include "cwTransformUpdater.h"
#include "cwSelectionManager.h"
class cwCamera;

/**
 * @brief The cwItem3DRepeater class
 *
 * Derived from cwAbstractPointManager. It:
 *  - Binds to a flat QAbstractItemModel
 *  - Creates one QML item per row using the component/source configured in the base
 *  - Maps model roles to item properties
 *  - Uses positionRole() to update "position3D"
 *  - Hooks items into cwTransformUpdater
 */
class CAVEWHERE_LIB_EXPORT cwItem3DRepeater : public cwAbstractPointManager
{
    Q_OBJECT
    QML_NAMED_ELEMENT(Item3DRepeater)

    Q_PROPERTY(QAbstractItemModel* model READ model WRITE setModel NOTIFY modelChanged)
    Q_PROPERTY(int positionRole READ positionRole WRITE setPositionRole NOTIFY positionRoleChanged)
    Q_PROPERTY(cwCamera* camera READ camera WRITE setCamera NOTIFY cameraChanged)

public:
    explicit cwItem3DRepeater(QQuickItem* parent = nullptr);
    ~cwItem3DRepeater() override;

    QAbstractItemModel* model() const;
    void setModel(QAbstractItemModel* model);

    int positionRole() const;
    void setPositionRole(int role);

    cwCamera* camera() const;
    void setCamera(cwCamera* camera);

signals:
    void modelChanged();
    void positionRoleChanged();
    void cameraChanged();

protected:
    // Implement abstract hooks from base
    void updateItemData(QQuickItem* item, int pointIndex) override;
    void updateItemPosition(QQuickItem* item, int pointIndex) override;

    // Lifecycle hooks to attach/detach to transform updater
    void onItemCreated(QQuickItem* item, int index) override;
    void onItemAboutToBeDestroyed(QQuickItem* item, int index) override;

    // Provide required properties prior to completeCreate()
    QVariantMap initialItemProperties(QQuickItem* item, int index) const override;

    void componentComplete() override;

private slots:
    void onRowsInserted(const QModelIndex& parent, int first, int last);
    void onRowsRemoved(const QModelIndex& parent, int first, int last);
    void onDataChanged(const QModelIndex& topLeft, const QModelIndex& bottomRight, const QList<int>& roles);
    void onModelReset();

private:
    bool isFlatListModel(const QModelIndex& parent) const;
    QModelIndex indexForRow(int row) const;

    QPointer<QAbstractItemModel> m_model;
    cwTransformUpdater* m_transformUpdater = nullptr;
    int m_positionRole = -1;

    bool m_removingItems = false;

};

#endif // CWITEM3DREPEATER_H



// /**************************************************************************
// **
// **    Copyright (C) 2025 by Philip Schuchardt
// **    www.cavewhere.com
// **
// **************************************************************************/

// #ifndef CWITEM3DREPEATER_H
// #define CWITEM3DREPEATER_H

// // Qt includes
// #include <QQuickItem>
// #include <QPointer>
// #include <QAbstractItemModel>
// #include <QQmlEngine>
// #include <QQmlComponent>
// #include <QVector3D>
// #include <QVector>
// #include <QUrl>

// // Our includes
// #include "cwTransformUpdater.h"
// #include "cwSelectionManager.h"
// class cwCamera;

// /**
//  * @brief The cwItem3DRepeater class
//  *
//  * Spawns a QML item per row of a QAbstractItemModel.
//  * - Each spawned item receives all role values as QML properties (roleNames() -> properties).
//  * - positionRole() identifies a role that converts to QVector3D for 3D placement.
//  * - Items are managed by cwTransformUpdater for screen-space projection.
//  */
// class CAVEWHERE_LIB_EXPORT cwItem3DRepeater : public QQuickItem
// {
//     Q_OBJECT
//     QML_NAMED_ELEMENT(Item3DRepeater)

//     Q_PROPERTY(QAbstractItemModel* model READ model WRITE setModel NOTIFY modelChanged)
//     Q_PROPERTY(QQmlComponent* component READ component WRITE setComponent NOTIFY componentChanged)
//     // Q_PROPERTY(QUrl qmlSource READ qmlSource WRITE setQmlSource NOTIFY qmlSourceChanged)
//     Q_PROPERTY(int positionRole READ positionRole WRITE setPositionRole NOTIFY positionRoleChanged)
//     Q_PROPERTY(cwCamera* camera READ camera WRITE setCamera NOTIFY cameraChanged)

// public:
//     explicit cwItem3DRepeater(QQuickItem* parent = nullptr);
//     ~cwItem3DRepeater();

//     QAbstractItemModel* model() const;
//     void setModel(QAbstractItemModel* model);

//     QQmlComponent* component() const;
//     void setComponent(QQmlComponent* component);

//     // QUrl qmlSource() const;
//     // void setQmlSource(const QUrl& source);

//     int positionRole() const;
//     void setPositionRole(int role);

//     cwCamera* camera() const;
//     void setCamera(cwCamera* camera);

//     Q_INVOKABLE void selectRow(int row);

// signals:
//     void modelChanged();
//     void qmlSourceChanged();
//     void positionRoleChanged();
//     void cameraChanged();
//     void componentChanged();

// protected:
//     void componentComplete() override;

// private slots:
//     void onRowsInserted(const QModelIndex& parent, int first, int last);
//     void onRowsRemoved(const QModelIndex& parent, int first, int last);
//     void onDataChanged(const QModelIndex& topLeft, const QModelIndex& bottomRight, const QList<int>& roles);
//     void onModelReset();
//     void onComponentStatusChanged(QQmlComponent::Status status);

// private:

//     QPointer<QAbstractItemModel> m_model;
//     cwTransformUpdater* m_transformUpdater;
//     cwSelectionManager* m_selectionManager;

//     // QUrl m_qmlSource;
//     // QQmlComponent* m_itemComponent = nullptr;
//     QPointer<QQmlComponent> m_component;
//     int m_positionRole = -1;

//     QVector<QQuickItem*> m_items;

//     void ensureInfrastructure();
//     // void ensureComponent();
//     bool ensureComponentReady() const;
//     void clearAll();
//     void rebuildAll();

//     bool isFlatListModel(const QModelIndex& parent) const;
//     QQuickItem* createItem(const QModelIndex& index);
//     void destroyItemAtRow(int row);
//     void spawnRows(int first, int last);
//     void updateItemFromIndex(int row);
//     void updateItemPosition(QQuickItem* item, const QModelIndex& index);
//     void exposeRolesToItem(QQuickItem* item, const QModelIndex& index);

//     QModelIndex indexForRow(int row) const;
// };

// #endif // CWITEM3DREPEATER_H
