#include "cwSanitizedNameSet.h"
#include "cwNameUtils.h"

QString cwSanitizedNameSet::toKey(const QString& name)
{
    return cwNameUtils::sanitizeFileName(name).toLower();
}

bool cwSanitizedNameSet::contains(const QString& name) const
{
    return m_names.contains(toKey(name));
}

bool cwSanitizedNameSet::wouldCollide(const QString& currentName, const QString& newName) const
{
    return wouldCollideKey(toKey(currentName), toKey(newName));
}

bool cwSanitizedNameSet::containsKey(const QString& key) const
{
    return m_names.contains(key);
}

bool cwSanitizedNameSet::wouldCollideKey(const QString& currentKey, const QString& newKey) const
{
    if (currentKey == newKey) {
        return false; // same sanitized form — can't collide with self
    }
    return m_names.contains(newKey);
}

void cwSanitizedNameSet::insert(const QString& name)
{
    m_names.insert(toKey(name));
}

void cwSanitizedNameSet::remove(const QString& name)
{
    m_names.remove(toKey(name));
}

void cwSanitizedNameSet::rename(const QString& oldName, const QString& newName)
{
    remove(oldName);
    insert(newName);
}

QString cwSanitizedNameSet::deduplicateName(const QString& proposedName) const
{
    const QString baseKey = toKey(proposedName);
    if (!m_names.contains(baseKey)) {
        return proposedName;
    }

    // Appending " N" to an already-clean name won't introduce forbidden chars,
    // so we can build candidate keys directly without re-sanitizing each time.
    for (int suffix = 2; suffix < 100000; ++suffix) {
        const QString candidate = proposedName + QStringLiteral(" %1").arg(suffix);
        const QString candidateKey = baseKey + QStringLiteral(" %1").arg(suffix);
        if (!m_names.contains(candidateKey)) {
            return candidate;
        }
    }

    return proposedName;
}

void cwSanitizedNameSet::clear()
{
    m_names.clear();
}
