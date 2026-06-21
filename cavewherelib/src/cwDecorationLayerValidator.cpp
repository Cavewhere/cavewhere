/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Our includes
#include "cwDecorationLayerValidator.h"
#include "cwDecorationLayer.h"
#include "cwPlacementRule.h"
#include "cwPlacementRuleRegistry.h"
#include "cwSymbologyErrorCodes.h"

namespace {

cwError makeError(SymbologyErrorCode code, cwError::ErrorType type, const QString &message)
{
    cwError error(message, type);
    error.setErrorTypeId(static_cast<int>(code));
    return error;
}

} // namespace

QList<cwError> cwDecorationLayerValidator::validate(const cwDecorationLayer &layer,
                                                   const cwPlacementRuleRegistry &registry,
                                                   const QSet<QString> &availableGlyphNames)
{
    QList<cwError> errors;

    // An empty stack is a legitimate "no ink" layer, not a malformed one.
    if (layer.rules.isEmpty()) {
        return errors;
    }

    int terminalCount = 0;
    int transformCount = 0;
    int generateCount = 0;
    int mutatePerLayerCount = 0;
    const cwPlacementRule *terminal = nullptr; // the first terminal — the one the layout keeps

    for (const cwPlacementRuleData &ruleData : layer.rules) {
        if (!ruleData.enabled) {
            continue;   // validate the effective stack: a disabled rule is as if absent
        }
        const cwPlacementRule *rule = registry.rule(ruleData.name);
        if (rule == nullptr) {
            errors.append(makeError(SymbologyErrorCode::UnknownRule, cwError::Warning,
                QStringLiteral("references unknown placement rule \"%1\"; it will be skipped")
                    .arg(ruleData.name)));
            continue;
        }
        switch (rule->stage()) {
        case cwPlacementRule::TransformStroke:
            transformCount++;
            break;
        case cwPlacementRule::Generate:
            generateCount++;
            break;
        case cwPlacementRule::MutatePerLayer:
            mutatePerLayerCount++;
            break;
        case cwPlacementRule::Terminal:
            terminalCount++;
            if (terminal == nullptr) {
                terminal = rule;
            }
            break;
        case cwPlacementRule::MutateScene:
            break; // iter 2; no rules register here yet
        }
    }

    // Fatal: two rules competing for a singleton slot. The layout silently keeps
    // the first by stage order, so which one wins is invisible to the author.
    if (terminalCount > 1) {
        errors.append(makeError(SymbologyErrorCode::TwoTerminals, cwError::Fatal,
            QStringLiteral("has %1 terminal rules; a layer must have at most one")
                .arg(terminalCount)));
    }
    if (transformCount > 1) {
        errors.append(makeError(SymbologyErrorCode::TwoTransformStrokes, cwError::Fatal,
            QStringLiteral("has %1 stroke-transform rules; a layer must have at most one")
                .arg(transformCount)));
    }

    if (terminal == nullptr) {
        // Rules present but nothing to produce geometry. The remaining checks all
        // key off the terminal's output kind, so there is nothing more to say.
        errors.append(makeError(SymbologyErrorCode::NoTerminalForRules, cwError::Warning,
            QStringLiteral("has rules but no terminal rule, so it draws nothing")));
        return errors;
    }

    switch (terminal->outputKind()) {
    case cwPlacementRule::OutputKind::Stamps:
        if (generateCount == 0) {
            errors.append(makeError(SymbologyErrorCode::StampsWithoutGenerate, cwError::Warning,
                QStringLiteral("stamps a glyph but has no Generate rule to place it, so it draws nothing")));
        }
        if (layer.glyphName.isEmpty()) {
            errors.append(makeError(SymbologyErrorCode::StampsWithoutGlyph, cwError::Warning,
                QStringLiteral("stamps a glyph but names no glyph, so it draws nothing")));
        } else if (!availableGlyphNames.contains(layer.glyphName)) {
            errors.append(makeError(SymbologyErrorCode::MissingGlyph, cwError::Warning,
                QStringLiteral("references glyph \"%1\", which is not in the palette")
                    .arg(layer.glyphName)));
        }
        break;
    case cwPlacementRule::OutputKind::Trace:
        if (generateCount > 0 || mutatePerLayerCount > 0) {
            errors.append(makeError(SymbologyErrorCode::DeadRulesUnderTrace, cwError::Warning,
                QStringLiteral("traces a line but also has stamp-placement rules, which have no effect")));
        }
        break;
    case cwPlacementRule::OutputKind::None:
        break;
    }

    return errors;
}
