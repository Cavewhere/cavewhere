/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWERRORTREELISTMODEL_H
#define CWERRORTREELISTMODEL_H

//Qt includes
#include <QAbstractListModel>
#include <QHash>

//Std includes
#include <vector>

//Our includes
#include "cwError.h"
class cwErrorListModel;

/**
 * The rows of an ordered list of cwErrorListModels, each source's rows after
 * the previous source's, as one flat list. cwErrorTreeModel's source model.
 *
 * Every source keeps its first row here, so a row finds its source by binary
 * search, and a batch of new sources joins in one insert.
 */
class cwErrorTreeListModel : public QAbstractListModel
{
    Q_OBJECT

public:
    explicit cwErrorTreeListModel(QObject* parent = nullptr);

    //! Replaces every source in one reset
    void setSources(const QList<cwErrorListModel*>& sources);
    //! Appends the new ones among \a sources after the last source, in one insert
    void appendSources(const QList<cwErrorListModel*>& sources);
    //! Removes the tracked ones among \a sources, one remove per run of adjacent sources
    void removeSources(const QList<cwErrorListModel*>& sources);

    QList<cwErrorListModel*> sources() const;
    //! The source that owns \a row, or nullptr when \a row is out of range
    cwErrorListModel* sourceForRow(int row) const;
    cwError errorAt(int row) const;

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    bool setData(const QModelIndex& index, const QVariant& value, int role) override;
    Qt::ItemFlags flags(const QModelIndex& index) const override;
    QHash<int, QByteArray> roleNames() const override;

private:
    struct Source {
        cwErrorListModel* model;
        int firstRow;
        int rowCount;
    };

    std::vector<Source> m_sources;
    //Each source's index in m_sources
    QHash<const QObject*, size_t> m_positions;
    int m_rowCount = 0;

    const Source* sourceAt(int row) const;
    QModelIndex sourceIndex(const QModelIndex& index) const;
    size_t senderPosition() const;
    void pushSource(cwErrorListModel* model);
    void resizeSenderSource(int rows);
    void connectSource(cwErrorListModel* source);
    void shiftFrom(size_t position, int rows);
    void reindexFrom(size_t position);

    void onSourceRowsAboutToBeInserted(const QModelIndex& parent, int first, int last);
    void onSourceRowsInserted(const QModelIndex& parent, int first, int last);
    void onSourceRowsAboutToBeRemoved(const QModelIndex& parent, int first, int last);
    void onSourceRowsRemoved(const QModelIndex& parent, int first, int last);
    void onSourceDataChanged(const QModelIndex& topLeft, const QModelIndex& bottomRight, const QList<int>& roles);
    void onSourceAboutToBeReset();
    void onSourceReset();
};

#endif // CWERRORTREELISTMODEL_H
