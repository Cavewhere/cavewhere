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

//Std includes
#include <algorithm>

namespace {

cwDecorationLayerError makeError(SymbologyErrorCode code, cwError::ErrorType type,
                                 const QString &message, int ruleIndex)
{
    cwError error(message, type);
    error.setErrorTypeId(static_cast<int>(code));
    return cwDecorationLayerError{error, ruleIndex};
}

// Layer-level problems carry no rule index; -1 routes them to the layer row.
cwDecorationLayerError makeLayerError(SymbologyErrorCode code, cwError::ErrorType type,
                                      const QString &message)
{
    return makeError(code, type, message, -1);
}

} // namespace

QList<cwDecorationLayerError> cwDecorationLayerValidator::validate(
    const cwDecorationLayer &layer,
    const cwPlacementRuleRegistry &registry,
    const QSet<QString> &availableGlyphNames)
{
    QList<cwDecorationLayerError> errors;

    // An empty stack is a legitimate "no ink" layer, not a malformed one.
    if (layer.rules.isEmpty()) {
        return errors;
    }

    int generateCount = 0;
    int mutatePerLayerCount = 0;
    int maxStageSeen = -1;                      // highest stage of an enabled, known rule so far
    const cwPlacementRule *terminal = nullptr;  // the first terminal — the one the layout keeps
    const cwPlacementRule *transform = nullptr; // the first stroke-transform — likewise

    for (int i = 0; i < layer.rules.size(); ++i) {
        const cwPlacementRuleData &ruleData = layer.rules.at(i);
        if (!ruleData.enabled) {
            continue;   // validate the effective stack: a disabled rule is as if absent
        }
        const cwPlacementRule *rule = registry.rule(ruleData.name);
        if (rule == nullptr) {
            errors.append(makeError(SymbologyErrorCode::UnknownRule, cwError::Warning,
                QStringLiteral("references unknown placement rule \"%1\"; it will be skipped")
                    .arg(ruleData.name), i));
            continue;   // unknown stage sorts last anyway; don't let it move maxStageSeen
        }

        // Out of order: a rule that runs before a higher stage already above it.
        // The layout stable-sorts by stage so it still renders correctly, but the
        // authored order no longer matches the pipeline the user reads top-down.
        const int stage = static_cast<int>(rule->stage());
        if (stage < maxStageSeen) {
            errors.append(makeError(SymbologyErrorCode::RulesOutOfOrder, cwError::Warning,
                QStringLiteral("rule \"%1\" runs before a later-stage rule above it; it is out of pipeline order")
                    .arg(rule->displayName()), i));
        }
        maxStageSeen = (std::max)(maxStageSeen, stage);

        switch (rule->stage()) {
        case cwPlacementRule::TransformStroke:
            // Fatal: a second rule competing for a singleton slot. The layout keeps
            // the first by stage order, so the extra one silently never runs.
            if (transform == nullptr) {
                transform = rule;
            } else {
                errors.append(makeError(SymbologyErrorCode::TwoTransformStrokes, cwError::Fatal,
                    QStringLiteral("stroke-transform rule \"%1\" won't run: the layer already has one above it")
                        .arg(rule->displayName()), i));
            }
            break;
        case cwPlacementRule::Generate:
            generateCount++;
            break;
        case cwPlacementRule::MutatePerLayer:
            mutatePerLayerCount++;
            break;
        case cwPlacementRule::Terminal:
            if (terminal == nullptr) {
                terminal = rule;
            } else {
                errors.append(makeError(SymbologyErrorCode::TwoTerminals, cwError::Fatal,
                    QStringLiteral("terminal rule \"%1\" won't run: the layer already has one above it")
                        .arg(rule->displayName()), i));
            }
            break;
        case cwPlacementRule::MutateScene:
            break; // iter 2; no rules register here yet
        }
    }

    if (terminal == nullptr) {
        // Rules present but nothing to produce geometry. The remaining checks all
        // key off the terminal's output kind, so there is nothing more to say.
        errors.append(makeLayerError(SymbologyErrorCode::NoTerminalForRules, cwError::Warning,
            QStringLiteral("has rules but no terminal rule, so it draws nothing")));
        return errors;
    }

    switch (terminal->outputKind()) {
    case cwPlacementRule::OutputKind::Stamps:
        if (generateCount == 0) {
            errors.append(makeLayerError(SymbologyErrorCode::StampsWithoutGenerate, cwError::Warning,
                QStringLiteral("stamps a glyph but has no Generate rule to place it, so it draws nothing")));
        }
        if (layer.glyphName.isEmpty()) {
            errors.append(makeLayerError(SymbologyErrorCode::StampsWithoutGlyph, cwError::Warning,
                QStringLiteral("stamps a glyph but names no glyph, so it draws nothing")));
        } else if (!availableGlyphNames.contains(layer.glyphName)) {
            errors.append(makeLayerError(SymbologyErrorCode::MissingGlyph, cwError::Warning,
                QStringLiteral("references glyph \"%1\", which is not in the palette")
                    .arg(layer.glyphName)));
        }
        break;
    case cwPlacementRule::OutputKind::Trace:
        if (generateCount > 0 || mutatePerLayerCount > 0) {
            errors.append(makeLayerError(SymbologyErrorCode::DeadRulesUnderTrace, cwError::Warning,
                QStringLiteral("traces a line but also has stamp-placement rules, which have no effect")));
        }
        break;
    case cwPlacementRule::OutputKind::None:
        break;
    }

    return errors;
}
