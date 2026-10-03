/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWATTACHEDFIXMODEL_H
#define CWATTACHEDFIXMODEL_H

//Qt includes
#include <QAbstractListModel>
#include <QList>
#include <QQmlEngine>

//Our includes
#include "cwExternalCenterline.h"
#include "cwGlobals.h"

/**
 * The stations a survey node's attached files fix themselves, one read-only row
 * per cwAttachedFix, in the order cwSurveyNode::setAttachedFixes() lists them.
 *
 * cwSurveyNode::fixStationTable concatenates this after the node's own
 * cwFixStationDiagnosticsModel, so a single Fix Stations delegate binds both.
 * The shared roles therefore reuse cwFixStationModel's role ids, and every
 * cwFixStationDiagnosticsModel role answers with its empty value, so a
 * delegate's required properties are defined on an attached row too.
 *
 * The rows come from the attach scan and the file is their source of truth, so
 * flags() leaves out Qt::ItemIsEditable and setData() refuses every edit.
 */
class CAVEWHERE_LIB_EXPORT cwAttachedFixModel : public QAbstractListModel
{
    Q_OBJECT
    QML_NAMED_ELEMENT(AttachedFixModel)
    QML_UNCREATABLE("Owned by SurveyNode; access via node.attachedFixes")

    Q_PROPERTY(int count READ count NOTIFY countChanged)

public:
    //! Numbered clear of cwFixStationModel's Qt::UserRole + 1… block and
    //! cwFixStationDiagnosticsModel's Qt::UserRole + 100… block: the
    //! concatenated table merges all three into one role hash.
    enum Roles {
        //! The attached entry file's name.
        SourceFileRole = Qt::UserRole + 200,
        //! Whether a file, not the user, owns the row: always true here, false
        //! on the node's own rows (cwFixStationDiagnosticsModel answers it).
        ReadOnlyRole
    };
    Q_ENUM(Roles)

    explicit cwAttachedFixModel(QObject* parent = nullptr);

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    Q_INVOKABLE QModelIndex index(int row, int column = 0, const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    Qt::ItemFlags flags(const QModelIndex& index) const override;
    QHash<int, QByteArray> roleNames() const override;

    int count() const { return static_cast<int>(m_fixes.size()); }

    const QList<cwAttachedFix>& attachedFixes() const { return m_fixes; }
    //! Replaces every row, resetting the model when the list differs.
    void setAttachedFixes(const QList<cwAttachedFix>& fixes);

    //! Row of the first fix with no coordinate system, -1 when every fix has one.
    int firstFixWithoutCoordinateSystem() const;

signals:
    void countChanged();

private:
    QList<cwAttachedFix> m_fixes;
};

#endif // CWATTACHEDFIXMODEL_H
