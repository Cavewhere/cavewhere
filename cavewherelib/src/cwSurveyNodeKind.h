/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWSURVEYNODEKIND_H
#define CWSURVEYNODEKIND_H

//Our includes
#include "CaveWhereLibExport.h"

//Qt includes
#include <QMetaEnum>
#include <QObject>
#include <QQmlEngine>

/**
 * What a survey node is called and drawn as. Display and reconcile only: no
 * behavior branches on it. Native nodes are Cave or Folder; the rest mirror one
 * level of an attached survey file.
 *
 * The enum lives in its own header, outside cwSurveyNode, because cwCaveData
 * names it and cwSurveyNode.h includes cwCaveData.h — the enum has to be
 * reachable from both. cwSurveyNode::Kind aliases it, so every caller keeps
 * spelling it the way it always has.
 */
namespace cwSurveyNodeKind {
Q_NAMESPACE_EXPORT(CAVEWHERE_LIB_EXPORT)
QML_NAMED_ELEMENT(SurveyNodeKind)

enum class Kind {
    Cave,
    Folder,
    CompassProject,
    CompassFile,
    WallsBook,
    SurvexFile,
    SurvexBlock
};
Q_ENUM_NS(Kind)

//! The Kind a saved kind field names, Cave for a value this build has no name
//! for — a newer file's node still loads as an ordinary cave.
inline Kind fromSavedValue(int savedKind)
{
    const bool known = QMetaEnum::fromType<Kind>().valueToKey(savedKind) != nullptr;
    return known ? static_cast<Kind>(savedKind) : Kind::Cave;
}
}

#endif // CWSURVEYNODEKIND_H
