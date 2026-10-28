/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWENTRANCE_H
#define CWENTRANCE_H

//Qt includes
#include <QMetaType>
#include <QString>
#include <QUuid>

//Our includes
#include "cwGlobals.h"

/**
 * One station a survey node marks as a cave entrance, with the optional label
 * its marker shows in place of the cave's name. Lives inside cwEntranceModel;
 * saved with the node beside its fix stations.
 */
class CAVEWHERE_LIB_EXPORT cwEntrance
{
    Q_GADGET

public:
    QUuid id() const { return m_id; }
    void setId(const QUuid& id) { m_id = id; }

    QString stationName() const { return m_stationName; }
    void setStationName(const QString& name) { m_stationName = name; }

    //! The name the user gave the entrance. Empty means its marker shows the
    //! cave's name.
    QString label() const { return m_label; }
    void setLabel(const QString& label) { m_label = label; }

    bool operator==(const cwEntrance& other) const = default;

private:
    QUuid m_id = QUuid::createUuid();
    QString m_stationName;
    QString m_label;
};

Q_DECLARE_METATYPE(cwEntrance)

#endif // CWENTRANCE_H
