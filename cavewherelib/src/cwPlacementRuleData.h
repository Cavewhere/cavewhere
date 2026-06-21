/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWPLACEMENTRULEDATA_H
#define CWPLACEMENTRULEDATA_H

//Qt includes
#include <QMetaType>
#include <QString>
#include <QVariant>

//Our includes
#include "CaveWhereLibExport.h"

// A placement-rule reference as stored on a decoration layer. The executable
// rule object is instantiated from this (name -> registry) at decoration-layout
// time; storing the (name, typed parameters) keeps cwLineBrush a pure
// implicitly-shared value type and lets unknown rules round-trip intact. The
// QVariant holds the decoded param struct (e.g. cwUniformSpacingParams), or
// raw QByteArray for unknown/malformed params; cwPlacementRuleParamsCodec
// bridges it to/from the persisted bytes at the IO boundary.
struct CAVEWHERE_LIB_EXPORT cwPlacementRuleData {
    QString  name;
    QVariant parameters;
    bool     enabled = true;   // false = present but skipped by layout + validator

    bool operator==(const cwPlacementRuleData &other) const = default;
};

Q_DECLARE_METATYPE(cwPlacementRuleData)

#endif // CWPLACEMENTRULEDATA_H
