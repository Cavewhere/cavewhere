//Our includes
#include "cwCavernNaming.h"

//Qt includes
#include <QLatin1Char>
#include <QSet>

namespace {

constexpr QLatin1Char kSeparator('.');
constexpr QLatin1Char kWordSeparator('_');

//! First suffix a colliding label takes, so the second "Fisher Ridge" is
//! fisher_ridge_2 rather than fisher_ridge_1.
constexpr int kFirstCollisionSuffix = 2;

}

QString cwCavernNaming::sanitizeToCavernIdentifier(const QString& name)
{
    //Decomposed so a Latin diacritic drops to its base letter ("rio", not
    //"r_o") instead of folding to a separator. Cavern's own command charset is
    //ASCII (survex commands.c sets a-z, A-Z, 0-9, '_' and '-'), so anything
    //that survives decomposition without an ASCII form still has to go.
    const QString decomposed = name.normalized(QString::NormalizationForm_KD);

    QString label;
    label.reserve(decomposed.size());

    bool pendingSeparator = false;
    for (const QChar character : decomposed) {
        if (character.isMark()) {
            continue; //a combining accent whose base letter was just kept
        }

        const QChar lower = character.toLower();
        const bool isIdentifierChar = (lower >= QLatin1Char('a') && lower <= QLatin1Char('z'))
                || (lower >= QLatin1Char('0') && lower <= QLatin1Char('9'));

        if (isIdentifierChar) {
            if (pendingSeparator) {
                label.append(kWordSeparator);
                pendingSeparator = false;
            }
            label.append(lower);
        } else if (!label.isEmpty()) {
            //Held rather than appended, so a run of punctuation collapses to one
            //separator and a trailing run adds none at all.
            pendingSeparator = true;
        }
    }

    if (label.isEmpty()) {
        return QStringLiteral("x"); //stands in for a name that sanitizes away to nothing
    }
    return label;
}

QHash<QUuid, QString> cwCavernNaming::scopeLabels(const QList<ScopeEntry>& siblings)
{
    QHash<QUuid, QString> labels;
    labels.reserve(siblings.size());

    QSet<QString> used;
    used.reserve(siblings.size());

    for (const ScopeEntry& entry : siblings) {
        const QString base = sanitizeToCavernIdentifier(entry.name);

        //Distinct names can sanitize to one identifier ("Big Cave" and
        //"Big-Cave"), and two scopes sharing a label would silently merge their
        //stations into one survey, so the collision has to be broken here.
        QString label = base;
        int suffix = kFirstCollisionSuffix;
        while (used.contains(label)) {
            label = base + kWordSeparator + QString::number(suffix);
            suffix++;
        }

        used.insert(label);
        labels.insert(entry.id, label);
    }

    return labels;
}

QString cwCavernNaming::scopePrefix(const QString& label)
{
    if (label.isEmpty()) {
        return QString();
    }
    return label + kSeparator;
}

QString cwCavernNaming::scopeHeadOf(const QString& scopedName)
{
    const qsizetype separator = scopedName.indexOf(kSeparator);
    if (separator <= 0) {
        return QString();
    }
    return scopedName.first(separator);
}

QString cwCavernNaming::removeScopeHead(const QString& scopedName)
{
    const qsizetype separator = scopedName.indexOf(kSeparator);
    if (separator <= 0) {
        return scopedName;
    }
    return scopedName.sliced(separator + 1);
}
