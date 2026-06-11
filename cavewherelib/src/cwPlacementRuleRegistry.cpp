/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Our includes
#include "cwPlacementRuleRegistry.h"
#include "cwPlacementRule.h"
#include "cwPlacementRules/cwUniformSpacingRule.h"
#include "cwPlacementRules/cwExplicitPointRule.h"
#include "cwPlacementRules/cwAlignToTangentRule.h"
#include "cwPlacementRules/cwRigidStampRule.h"
#include "cwPlacementRules/cwJointedStampRule.h"
#include "cwPlacementRules/cwBendingStampRule.h"
#include "cwPlacementRules/cwTraceOffsetPolylineRule.h"

cwPlacementRuleRegistry::cwPlacementRuleRegistry()
{
    add(std::make_unique<cwUniformSpacingRule>());
    add(std::make_unique<cwExplicitPointRule>());
    add(std::make_unique<cwAlignToTangentRule>());
    add(std::make_unique<cwRigidStampRule>());
    add(std::make_unique<cwJointedStampRule>());
    add(std::make_unique<cwBendingStampRule>());
    add(std::make_unique<cwTraceOffsetPolylineRule>());
}

void cwPlacementRuleRegistry::add(std::unique_ptr<cwPlacementRule> rule)
{
    m_byName.insert(rule->displayName(), rule.get());
    m_rules.push_back(std::move(rule));
}

const cwPlacementRuleRegistry &cwPlacementRuleRegistry::instance()
{
    static const cwPlacementRuleRegistry registry;
    return registry;
}

const cwPlacementRule *cwPlacementRuleRegistry::rule(const QString &name) const
{
    return m_byName.value(name, nullptr);
}
