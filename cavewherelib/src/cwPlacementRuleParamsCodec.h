/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWPLACEMENTRULEPARAMSCODEC_H
#define CWPLACEMENTRULEPARAMSCODEC_H

//Our includes
#include "CaveWhereLibExport.h"

//Qt includes
#include <QByteArray>
#include <QString>
#include <QVariant>

// Bidirectional bridge between persisted PlacementRule.params bytes and the
// typed, proto-free param structs the rules read. The ONLY proto-aware module
// in the rule-params path; see the codec behavior matrix in
// plans/COMMIT_B_RULE_PARAMS_PLAN.md.
namespace cwPlacementRuleParamsCodec {
    CAVEWHERE_LIB_EXPORT QVariant   decode(const QString &name, const QByteArray &params);
    CAVEWHERE_LIB_EXPORT QByteArray encode(const QString &name, const QVariant &params);
}

#endif // CWPLACEMENTRULEPARAMSCODEC_H
