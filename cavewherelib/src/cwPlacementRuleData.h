/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWPLACEMENTRULEDATA_H
#define CWPLACEMENTRULEDATA_H

//Qt includes
#include <QByteArray>
#include <QMetaType>
#include <QString>

//Our includes
#include "CaveWhereLibExport.h"

// A placement-rule reference as stored on a decoration layer. The executable
// rule object is instantiated from this (name -> registry) at decoration-layout
// time; storing the raw (name, params) keeps cwLineBrush a pure
// implicitly-shared value type and lets unknown rules round-trip intact.
// The live cwPlacementRule registry lands in a later phase.
struct CAVEWHERE_LIB_EXPORT cwPlacementRuleData {
    QString    name;
    QByteArray params;

    bool operator==(const cwPlacementRuleData &other) const = default;
};

Q_DECLARE_METATYPE(cwPlacementRuleData)

#endif // CWPLACEMENTRULEDATA_H
