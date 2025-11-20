/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWCAVINGREGION_H
#define CWCAVINGREGION_H

//Qt includes
#include <QObject>
#include <QList>
#include <QUndoCommand>
#include <QWeakPointer>
#include <QAbstractListModel>
#include <QDebug>
#include <QSharedPointer>
#include <QQmlEngine>
#include <QObjectBindableProperty>

//Our includes
class cwCave;
class cwProject;
#include "cwCavingRegionData.h"
#include "cwUndoer.h"
#include "cwGlobals.h"


class CAVEWHERE_LIB_EXPORT cwCavingRegion : public QAbstractListModel, public cwUndoer
{
    Q_OBJECT
    QML_NAMED_ELEMENT(CavingRegion)

    Q_PROPERTY(QString name READ name WRITE setName NOTIFY nameChanged BINDABLE bindableName)
    Q_PROPERTY(int caveCount READ caveCount NOTIFY caveCountChanged)

public:
    enum Roles {
        CaveObjectRole
    };
    Q_ENUM(Roles)

    explicit cwCavingRegion(QObject *parent = nullptr);
    // cwCavingRegion(const cwCavingRegion& object);
    // cwCavingRegion& operator=(const cwCavingRegion& object);
//    ~cwCavingRegion() { qDebug() << "Deleted: " << this; }

    QString name() const { return m_name.value(); }
    void setName(const QString& name) { m_name = name; }
    QBindable<QString> bindableName() { return &m_name; }

    bool hasCaves() const;
    Q_INVOKABLE int caveCount() const;
    Q_INVOKABLE cwCave* cave(int index) const;
    QList<cwCave*> caves() const;

    Q_INVOKABLE int rowCount(const QModelIndex &parent = QModelIndex()) const;
    Q_INVOKABLE QVariant data(const QModelIndex &index, int role) const;
    QHash<int, QByteArray> roleNames() const;
    Q_INVOKABLE QModelIndex index(int row, int column = 0, const QModelIndex &parent = QModelIndex()) const;

    Q_INVOKABLE void addCave(cwCave* cave = nullptr);
    Q_INVOKABLE void addCaves(QList<cwCave*> cave);
    void insertCave(int index, cwCave* cave);
    Q_INVOKABLE void removeCave(int index);
    void removeCaves(int beginIndex, int endIndex);
    void clearCaves();

    int indexOf(cwCave* cave);

    cwProject* parentProject() const;

    void setData(const cwCavingRegionData &data);
    cwCavingRegionData data() const;

signals:
    void nameChanged();

    void beginInsertCaves(int begin, int end);
    void insertedCaves(int begin, int end);

    void beginRemoveCaves(int begin, int end);
    void removedCaves(int begin, int end);

    void caveCountChanged();

public slots:

protected:
    QList<cwCave*> m_caves;

    virtual void setUndoStackForChildren();

private:
    Q_OBJECT_BINDABLE_PROPERTY_WITH_ARGS(cwCavingRegion, QString, m_name, QString(), &cwCavingRegion::nameChanged);

    // cwCavingRegion& copy(const cwCavingRegion& object);

    void unparentCave(cwCave* cave);
    void addCaveHelper();

    ////////////////////// Undo Redo commands ///////////////////////////////////
    class InsertRemoveCave : public QUndoCommand {
    public:
        InsertRemoveCave(cwCavingRegion* region, int beginIndex, int endIndex);
        ~InsertRemoveCave();

    protected:
        void insertCaves();
        void removeCaves();

        QList< cwCave* > Caves;
    private:
        cwCavingRegion* Region;
        int BeginIndex;
        int EndIndex;
        bool OwnsCaves; //!< If the undo command own the caves, ie, it'll delete them
    };

    class InsertCaveCommand : public InsertRemoveCave {

    public:
        InsertCaveCommand(cwCavingRegion* parentRegion, cwCave* cave, int index);
        InsertCaveCommand(cwCavingRegion* parentRegion, QList<cwCave*> cave, int index);
        virtual void redo();
        virtual void undo();
    };

    class RemoveCaveCommand : public InsertRemoveCave {
    public:
        RemoveCaveCommand(cwCavingRegion* region, int beginIndex, int endIndex);
        virtual void redo();
        virtual void undo();
    };
};

typedef QSharedPointer<cwCavingRegion> cwCavingRegionPtr;

/**
  \brief Get's the number of caves in the region
  */
inline int cwCavingRegion::caveCount() const {
    return m_caves.count();
}

/**
  \brief Returns true if the region has at least on cave, otherwise false
  */
inline bool cwCavingRegion::hasCaves() const {
    return !m_caves.isEmpty();
}

/**
  \brief Get's a cave at index
  */
inline cwCave* cwCavingRegion::cave(int index) const {
    if(index < 0 || index >= m_caves.size()) { return nullptr; }
    return m_caves[index];
}

/**
  \brief Gets all the caves in the region
  */
inline QList<cwCave*> cwCavingRegion::caves() const {
    return m_caves;
}

#endif // CWCAVINGREGION_H
