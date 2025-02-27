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

//Our includes
class cwCave;
class cwProject;
#include "cwUndoer.h"
#include "cwGlobals.h"

class CAVEWHERE_LIB_EXPORT cwCavingRegion : public QAbstractListModel, public cwUndoer
{
    Q_OBJECT
    QML_NAMED_ELEMENT(CavingRegion)

    Q_PROPERTY(int caveCount READ caveCount NOTIFY caveCountChanged)

public:
    enum Roles {
        CaveObjectRole
    };
    Q_ENUM(Roles)

    // enum class Columns : int {
    //     Name,
    //     Length,
    //     Depth,

    // }

    explicit cwCavingRegion(QObject *parent = nullptr);
    cwCavingRegion(const cwCavingRegion& object);
    cwCavingRegion& operator=(const cwCavingRegion& object);
//    ~cwCavingRegion() { qDebug() << "Deleted: " << this; }

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
signals:
    void beginInsertCaves(int begin, int end);
    void insertedCaves(int begin, int end);

    void beginRemoveCaves(int begin, int end);
    void removedCaves(int begin, int end);

    void caveCountChanged();

public slots:

protected:
    QList<cwCave*> Caves;

    virtual void setUndoStackForChildren();

private:
    cwCavingRegion& copy(const cwCavingRegion& object);

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
    return Caves.count();
}

/**
  \brief Returns true if the region has at least on cave, otherwise false
  */
inline bool cwCavingRegion::hasCaves() const {
    return !Caves.isEmpty();
}

/**
  \brief Get's a cave at index
  */
inline cwCave* cwCavingRegion::cave(int index) const {
    if(index < 0 || index >= Caves.size()) { return nullptr; }
    return Caves[index];
}

/**
  \brief Gets all the caves in the region
  */
inline QList<cwCave*> cwCavingRegion::caves() const {
    return Caves;
}

#endif // CWCAVINGREGION_H
