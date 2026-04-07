#pragma once

#include "cwGlobals.h"

#include <QString>
#include <QSet>

/**
 * Maintains a set of sanitized (filesystem-safe, case-folded) names.
 * Used by parent containers to provide O(1) sibling-name collision checks.
 */
class CAVEWHERE_LIB_EXPORT cwSanitizedNameSet {
public:
    bool contains(const QString& name) const;

    // Returns true if renaming from currentName to newName would collide
    // with an existing entry that isn't currentName itself.
    bool wouldCollide(const QString& currentName, const QString& newName) const;

    // Key-based overloads to avoid redundant sanitization when the caller
    // has already computed a sanitized key via toKey().
    bool containsKey(const QString& key) const;
    bool wouldCollideKey(const QString& currentKey, const QString& newKey) const;

    void insert(const QString& name);
    void remove(const QString& name);
    void rename(const QString& oldName, const QString& newName);

    // Returns proposedName if unique, otherwise appends " 2", " 3", etc.
    QString deduplicateName(const QString& proposedName) const;

    void clear();

    static QString toKey(const QString& name);

private:
    QSet<QString> m_names;
};
