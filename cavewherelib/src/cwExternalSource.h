/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWEXTERNALSOURCE_H
#define CWEXTERNALSOURCE_H

//Qt includes
#include <QMetaType>
#include <QStringList>
#include <QUuid>

//Our includes
#include "cwGlobals.h"

/**
 * One attached survey source (a Compass .mak, a Walls .wpj, a Survex master):
 * the root node its scan builds and the derived nodes and trips the user hid.
 *
 * id is the cwSurveyNode::sourceId() of the root and of every derived node.
 * excludedSourcePaths lists the cwSurveyNode::sourcePath() or
 * cwTrip::sourcePath() of each hidden item; a hidden node hides its derived
 * descendants with it.
 *
 * Value type (copyable, equality-comparable). The cwCavingRegion holds every
 * source in its cwExternalSourceModel.
 */
class CAVEWHERE_LIB_EXPORT cwExternalSource
{
    Q_GADGET
    Q_PROPERTY(QUuid id READ id WRITE setId FINAL)
    Q_PROPERTY(QUuid rootNodeId READ rootNodeId WRITE setRootNodeId FINAL)
    Q_PROPERTY(QStringList excludedSourcePaths READ excludedSourcePaths WRITE setExcludedSourcePaths FINAL)

public:
    cwExternalSource() = default;
    cwExternalSource(const QUuid& id,
                     const QUuid& rootNodeId,
                     const QStringList& excludedSourcePaths = {}) :
        m_id(id),
        m_rootNodeId(rootNodeId),
        m_excludedSourcePaths(excludedSourcePaths)
    {}

    QUuid id() const { return m_id; }
    void setId(const QUuid& id) { m_id = id; }

    QUuid rootNodeId() const { return m_rootNodeId; }
    void setRootNodeId(const QUuid& rootNodeId) { m_rootNodeId = rootNodeId; }

    QStringList excludedSourcePaths() const { return m_excludedSourcePaths; }
    void setExcludedSourcePaths(const QStringList& paths) { m_excludedSourcePaths = paths; }

    bool operator==(const cwExternalSource& other) const = default;

private:
    QUuid m_id;
    QUuid m_rootNodeId;
    QStringList m_excludedSourcePaths;
};

Q_DECLARE_METATYPE(cwExternalSource)

#endif // CWEXTERNALSOURCE_H
