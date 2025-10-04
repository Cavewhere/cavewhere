/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWREGIONTREEMODEL_H
#define CWREGIONTREEMODEL_H

//Our includes
class cwCavingRegion;
class cwTrip;
class cwCave;
class cwNote;
class cwScrap;
class cwNoteLiDAR;
#include "cwSurveyNoteModel.h"
#include "cwSurveyNoteLiDARModel.h"
#include "cwGlobals.h"

//Qt includes
#include <QAbstractItemModel>
#include <QtGlobal>
#include <QDebug>
#include <QPointer>
#include <QQmlEngine>

#define INVOKE_MEMBER(object,ptrToMember)  ((*object).*(ptrToMember))

/**
 * @brief The cwRegionTreeModel class
 *
 * The regionTreeModel allows for global access to cwRegion via a QAbstractItemModel tree. Currently
 * the tree model supports signaling support for add and removing cwCave, cwTrip, cwNote, cwScrap.
 */
class CAVEWHERE_LIB_EXPORT cwRegionTreeModel : public QAbstractItemModel
{
    Q_OBJECT
    QML_NAMED_ELEMENT(RegionTreeModel)

public:
    enum RoleItem {
        TypeRole, //Returns an ItemType
        ObjectRole, //For exctracting the object
    };
    Q_ENUM(RoleItem)

    enum ItemType {
        RegionType,
        CaveType,
        TripType,
        NoteType,
        ScrapType,
        NotesType,
        NotesLiDARType,
        NoteLiDARType
    };
    Q_ENUM(ItemType)

    enum class TripRows : int {
        NotesModel,
        NotesLiDARModel,

        //Add more rows above this
        NumberOfRows
    };

    explicit cwRegionTreeModel(QObject *parent = 0);

    void setCavingRegion(cwCavingRegion* region);
    cwCavingRegion* cavingRegion() const;

    Q_INVOKABLE QModelIndex index ( int row, int column, const QModelIndex & parent ) const;
    QModelIndex index (cwCave* cave) const;
    QModelIndex index (cwTrip* trip) const;
    QModelIndex index (cwNote* note) const;
    QModelIndex index (cwScrap* scrap) const;
    QModelIndex index (cwNoteLiDAR* noteLiDAR) const;
    QModelIndex index (cwSurveyNoteModel* model) const;
    QModelIndex index (cwSurveyNoteLiDARModel* model) const;

    QModelIndex parent ( const QModelIndex & index ) const;
    int rowCount ( const QModelIndex & parent = QModelIndex() ) const;
    int columnCount ( const QModelIndex & parent = QModelIndex() ) const;
    Q_INVOKABLE QVariant data ( const QModelIndex & index, int role) const;

    Q_INVOKABLE cwTrip* trip(const QModelIndex& index) const;
    Q_INVOKABLE cwCave* cave(const QModelIndex& index) const;
    Q_INVOKABLE cwNote* note(const QModelIndex& index) const;
    Q_INVOKABLE cwScrap* scrap(const QModelIndex& index) const;
    Q_INVOKABLE cwSurveyNoteModel* notesModel(const QModelIndex& index) const;
    Q_INVOKABLE cwSurveyNoteLiDARModel* notesLiDARModel(const QModelIndex& index) const;
    Q_INVOKABLE cwNoteLiDAR* noteLiDAR(const QModelIndex& index) const;
    Q_INVOKABLE QObject* object(const QModelIndex& index) const;


    Q_INVOKABLE bool isScrap(const QModelIndex& index) const;
    Q_INVOKABLE bool isNote(const QModelIndex& index) const;
    Q_INVOKABLE bool isTrip(const QModelIndex& index) const;
    Q_INVOKABLE bool isCave(const QModelIndex& index) const;
    Q_INVOKABLE bool isRegion(const QModelIndex& index) const;
    Q_INVOKABLE bool isNotes(const QModelIndex& index) const { return index.data(TypeRole).toInt() == NotesType; }
    Q_INVOKABLE bool isNotesLiDAR(const QModelIndex& index) const { return index.data(TypeRole).toInt() == NotesLiDARType; }
    Q_INVOKABLE bool isNoteLiDAR(const QModelIndex& index) const { return index.data(TypeRole).toInt() == NoteLiDARType; }

    template <typename ReturnType, typename GetFunc>
    QList<ReturnType> objects(const QModelIndex& parent,
                              int begin,
                              int end,
                              GetFunc func) const
    {
        QList<ReturnType> objects;
        for(int i = begin; i <= end; i++) {
            auto rowIndex = index(i, 0, parent);
            ReturnType obj = get<ReturnType>(rowIndex, func);
            if(obj) {
                objects.append(obj);
            }
        }

        return objects;
    }

    template <typename ReturnType, typename GetFunc>
    QList<ReturnType> all(const QModelIndex& parent, GetFunc func) const {
        QList<ReturnType> objects;
        for(int row = 0; row < rowCount(parent); row++) {
            auto rowIndex = index(row, 0, parent);

            //This assert detect infinite recursive calls to all
            Q_ASSERT(rowIndex != QModelIndex());

            auto obj = get<ReturnType>(rowIndex, func);
            if(obj) {
                objects.append(obj);
            }
            objects += all<ReturnType>(rowIndex, func);
        }
        return objects;
    }

    virtual QHash<int, QByteArray> roleNames() const;

signals:

public slots:

private slots:
    void beginInsertCaves(QModelIndex parent, int begin, int end);
    void insertedCaves(QModelIndex parent, int begin, int end);
    void beginRemoveCaves(QModelIndex parent, int begin, int end);
    void removedCaves(QModelIndex parent, int begin, int end);

    void beginInsertTrips(QModelIndex parent, int begin, int end);
    void insertedTrips(QModelIndex parent, int begin, int end);
    void beginRemoveTrips(QModelIndex parent, int begin, int end);
    void removedTrips(QModelIndex parent, int begin, int end);

    void beginInsertNotes(QModelIndex parent, int begin, int end);
    void insertedNotes(QModelIndex parent, int begin, int end);
    void beginRemoveNotes(QModelIndex parent, int begin, int end);
    void removeNotes(QModelIndex parent, int begin, int end);

    void beginInsertScraps(int begin, int end);
    void insertedScraps(int begin, int end);
    void beginRemoveScraps(int begin, int end);
    void removedScraps(int begin, int end);

private:

    QPointer<cwCavingRegion> Region;

    void addCaveConnections(int beginIndex, int endIndex);
    void removeCaveConnections(int beginIndex, int endIndex);

    void addTripConnections(cwCave* parentCave, int beginIndex, int endIndex);
    void removeTripConnections(cwCave* parentCave, int beginIndex, int endIndex);

    void addNoteConnections(cwTrip* parentTrip, int beginIndex, int endIndex);
    void removeNoteConnections(cwTrip* parentTrip, int beginIndex, int endIndex);

    void beginRemoveTrips(cwCave* parentCave, int begin, int end);
    void beginRemoveNotes(cwTrip* parentTrip, int begin, int end);
    void beginRemoveScraps(cwNote* parentNote, int  begin, int end);

    void insertedTrips(cwCave* parentCave, int begin, int end);
    void insertedNotes(cwTrip* parentTrip, int begin, int end);
    void insertedScraps(cwNote* parentNote, int begin, int end);

    template <typename ReturnType, typename GetFunc>
    ReturnType get(const QModelIndex& rowIndex, GetFunc func) const {
        return INVOKE_MEMBER(this, func)(rowIndex); //std::invoke(func, this, rowIndex);
    };

};


/**
  \brief Checks if index is a trip, returns true if it is, false if it isn't
  */
inline bool cwRegionTreeModel::isTrip(const QModelIndex &index) const {
    return index.data(TypeRole).toInt() == TripType;
}

/**
  \brief Checks if index is a cave, returns true if it is, false if it isn't
  */
inline bool cwRegionTreeModel::isCave(const QModelIndex &index) const {
    return index.data(TypeRole).toInt() == CaveType;
}

/**
  \brief Checks if index is a region, return true if it is, false if it isn't
  */
inline bool cwRegionTreeModel::isRegion(const QModelIndex &index) const {
    return index == QModelIndex();
}

/**
 * @brief cwRegionTreeModel::isScrap
 * @param index
 * @return
 */
inline bool cwRegionTreeModel::isScrap(const QModelIndex &index) const
{
    return index.data(TypeRole).toInt() == ScrapType;
}

/**
 * @brief cwRegionTreeModel::isNote
 * @param index
 * @return
 */
inline bool cwRegionTreeModel::isNote(const QModelIndex &index) const
{
    return index.data(TypeRole).toInt() == NoteType;
}





#endif // CWREGIONTREEMODEL_H
