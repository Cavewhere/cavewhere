/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Our includes
#include "cwSceneVisibility.h"

cwVisibilitySnapshot::cwVisibilitySnapshot(std::shared_ptr<const Data> data) :
    m_data(std::move(data))
{
}

quint64 cwVisibilitySnapshot::version() const
{
    return m_data == nullptr ? 0 : m_data->version;
}

bool cwVisibilitySnapshot::objectVisible(cwRenderObjectId id) const
{
    return m_data == nullptr || !m_data->hiddenObjects.contains(id);
}

bool cwVisibilitySnapshot::subVisible(cwRenderObjectId id, uint64_t subId) const
{
    if (m_data == nullptr) {
        return true;
    }
    if (m_data->hiddenObjects.contains(id)) {
        return false;
    }
    const auto it = m_data->subEntries.constFind({id, subId});
    return it == m_data->subEntries.constEnd() ? true : it->visible;
}

const QVector<quint8>* cwVisibilitySnapshot::mask(cwRenderObjectId id, uint64_t subId) const
{
    if (m_data == nullptr) {
        return nullptr;
    }
    const auto it = m_data->subEntries.constFind({id, subId});
    if (it == m_data->subEntries.constEnd() || it->mask.isEmpty()) {
        return nullptr;
    }
    return &it->mask;
}

quint64 cwVisibilitySnapshot::entryVersion(cwRenderObjectId id, uint64_t subId) const
{
    if (m_data == nullptr) {
        return 0;
    }
    const auto it = m_data->subEntries.constFind({id, subId});
    return it == m_data->subEntries.constEnd() ? 0 : it->entryVersion;
}

cwSceneVisibility::cwSceneVisibility(QObject* parent) :
    QObject(parent)
{
}

// Every mutator probes with const lookups before touching a container: a
// non-const find()/begin()/remove() detaches the live state (a deep copy)
// when a snapshot is outstanding, which would make even no-op writes pay
// O(n). Mutating calls run only on the effective-change branches.
void cwSceneVisibility::setObjectVisible(cwRenderObjectId id, bool visible)
{
    const bool currentlyHidden = m_live.hiddenObjects.contains(id);
    if (visible) {
        if (!currentlyHidden) {
            return;
        }
        m_live.hiddenObjects.remove(id);
    } else {
        if (currentlyHidden) {
            return;
        }
        m_live.hiddenObjects.insert(id);
    }
    touch();
}

void cwSceneVisibility::setSubVisible(cwRenderObjectId id, uint64_t subId, bool visible)
{
    const SubKey key{id, subId};
    const auto probe = m_live.subEntries.constFind(key);
    if (probe == m_live.subEntries.constEnd()) {
        if (visible) {
            return;
        }
        Entry entry;
        entry.visible = false;
        entry.entryVersion = m_live.version + 1;
        m_live.subEntries.insert(key, entry);
    } else {
        if (probe->visible == visible) {
            return;
        }
        if (visible && probe->mask.isEmpty()) {
            m_live.subEntries.remove(key);
        } else {
            const auto it = m_live.subEntries.find(key);
            it->visible = visible;
            it->entryVersion = m_live.version + 1;
        }
    }
    touch();
}

void cwSceneVisibility::setMask(cwRenderObjectId id, uint64_t subId, QVector<quint8> mask)
{
    const SubKey key{id, subId};
    const auto probe = m_live.subEntries.constFind(key);
    if (probe == m_live.subEntries.constEnd()) {
        if (mask.isEmpty()) {
            return;
        }
        Entry entry;
        entry.mask = std::move(mask);
        entry.entryVersion = m_live.version + 1;
        m_live.subEntries.insert(key, entry);
    } else {
        if (probe->mask == mask) {
            return;
        }
        if (mask.isEmpty() && probe->visible) {
            m_live.subEntries.remove(key);
        } else {
            const auto it = m_live.subEntries.find(key);
            it->mask = std::move(mask);
            it->entryVersion = m_live.version + 1;
        }
    }
    touch();
}

void cwSceneVisibility::removeObject(cwRenderObjectId id)
{
    const bool hadObject = m_live.hiddenObjects.contains(id);
    bool hasSubEntries = false;
    for (auto it = m_live.subEntries.cbegin(); it != m_live.subEntries.cend(); ++it) {
        if (it.key().first == id) {
            hasSubEntries = true;
            break;
        }
    }
    if (!hadObject && !hasSubEntries) {
        return;
    }

    if (hadObject) {
        m_live.hiddenObjects.remove(id);
    }
    if (hasSubEntries) {
        for (auto it = m_live.subEntries.begin(); it != m_live.subEntries.end();) {
            if (it.key().first == id) {
                it = m_live.subEntries.erase(it);
            } else {
                ++it;
            }
        }
    }
    touch();
}

void cwSceneVisibility::removeSub(cwRenderObjectId id, uint64_t subId)
{
    const SubKey key{id, subId};
    if (!m_live.subEntries.contains(key)) {
        return;
    }
    m_live.subEntries.remove(key);
    touch();
}

cwVisibilitySnapshot cwSceneVisibility::snapshot() const
{
    if (m_cached == nullptr) {
        m_cached = std::make_shared<const cwVisibilitySnapshot::Data>(m_live);
    }
    return cwVisibilitySnapshot(m_cached);
}

quint64 cwSceneVisibility::version() const
{
    return m_live.version;
}

void cwSceneVisibility::touch()
{
    ++m_live.version;
    m_cached.reset();
    emit changed();
}
