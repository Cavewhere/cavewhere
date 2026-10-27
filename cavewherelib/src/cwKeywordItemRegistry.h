/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWKEYWORDITEMREGISTRY_H
#define CWKEYWORDITEMREGISTRY_H

//Qt includes
#include <QHash>
#include <QList>
#include <QPointer>

//Our includes
#include "cwKeywordItem.h"
#include "cwKeywordItemModel.h"

/**
 * @brief Owns the lifecycle of per-entity cwKeywordItems published into a shared
 * cwKeywordItemModel.
 *
 * The "register one cwKeywordItem per entity, swap models safely, sync-delete in
 * the destructor" dance is reimplemented across several views/managers. This helper
 * owns the mechanical part of it so each call site keeps only its own predicate
 * (when does an entity deserve a keyword item) and factory (which keyword model to
 * extend, which object to target).
 *
 * Ownership: cwKeywordItemModel::addItems reparents the item to the model, so the
 * item outlives the registry's owner unless removed here. removeItem() detaches it
 * from the model; the registry then deletes it. clear() (teardown) deletes
 * synchronously because the event loop may not run again to drain deleteLater;
 * drop() (live removal) defers.
 *
 * Teardown ordering: cwKeywordItem::setObject() connects the target's destroyed()
 * signal to self-delete the item. The registry therefore always erases an item
 * from its map (drop()/clear()) and destroys it *before* its target is destroyed,
 * so the target's destroyed() handler never races a stale registry pointer.
 *
 * Key is the lookup key itself: an entity pointer (cwScrap*, cwTrip*, ...) for
 * pointer-identity sites, or a stable value key (QUuid, ...) for sites that own
 * one. It only needs qHash(Key) + operator==. For a pointer Key a forward
 * declaration of the pointee still suffices, since only the pointer is stored.
 */
template <class Key>
class cwKeywordItemRegistry
{
public:
    cwKeywordItemRegistry() = default;
    ~cwKeywordItemRegistry() { clear(); }

    cwKeywordItemRegistry(const cwKeywordItemRegistry&) = delete;
    cwKeywordItemRegistry& operator=(const cwKeywordItemRegistry&) = delete;

    cwKeywordItemModel* model() const { return m_model; }

    /**
     * Tears down every item registered with the old model, then adopts the new
     * one. The owner is responsible for re-registering surviving entries against
     * the new model (it owns the predicate), so nothing is rebuilt here.
     */
    void setModel(cwKeywordItemModel* model) {
        removeAllFromModel(DeleteMode::Deferred);
        m_items.clear();
        m_model = model;
    }

    /**
     * Ensures one keyword item is registered for key. Idempotent: returns the
     * existing item if present, otherwise builds one with makeItem (which must
     * set the item's extension and target object before returning it) and adds it
     * to the model. Returns nullptr when no model is set.
     */
    template <class Factory>
    cwKeywordItem* ensure(const Key& key, Factory&& makeItem) {
        return ensureAll(QList<Key>{key}, [&makeItem](const Key&) { return makeItem(); }).value(0, nullptr);
    }

    /**
     * ensure() for many keys at once: returns one item per key, in key order,
     * and adds every newly built item to the model in one insert. makeItem(key)
     * runs once per key that has no item yet; a repeated key shares its first
     * occurrence's item. Returns an empty list when no model is set.
     */
    template <class Factory>
    QList<cwKeywordItem*> ensureAll(const QList<Key>& keys, Factory&& makeItem) {
        if(m_model.isNull()) {
            return {};
        }

        QList<cwKeywordItem*> items;
        items.reserve(keys.size());
        QList<cwKeywordItem*> added;
        for(const Key& key : keys) {
            auto it = m_items.constFind(key);
            if(it != m_items.constEnd() && it.value() != nullptr) {
                items.append(it.value());
                continue;
            }

            cwKeywordItem* item = makeItem(key);
            m_items.insert(key, item);
            added.append(item);
            items.append(item);
        }

        // addItems after every factory has called setObject(), so the model's
        // resolveVisibility sees each target when it fires.
        m_model->addItems(added);
        return items;
    }

    /**
     * Removes and deletes (deferred) the keyword item for key, if any.
     */
    void drop(const Key& key) {
        auto it = m_items.find(key);
        if(it == m_items.end()) {
            return;
        }
        destroyItem(it.value(), DeleteMode::Deferred);
        m_items.erase(it);
    }

    /**
     * Removes and deletes (synchronous) every keyword item. For teardown/model
     * swap, where the event loop may not run again to drain deleteLater.
     */
    void clear() {
        removeAllFromModel(DeleteMode::Immediate);
        m_items.clear();
    }

private:
    enum class DeleteMode { Deferred, Immediate };

    void destroyItem(cwKeywordItem* item, DeleteMode mode) {
        if(item == nullptr) {
            return;
        }
        if(!m_model.isNull()) {
            m_model->removeItem(item);
        }
        if(mode == DeleteMode::Immediate) {
            delete item;
        } else {
            item->deleteLater();
        }
    }

    void removeAllFromModel(DeleteMode mode) {
        for(auto it = m_items.constBegin(); it != m_items.constEnd(); ++it) {
            destroyItem(it.value(), mode);
        }
    }

    QPointer<cwKeywordItemModel> m_model;
    QHash<Key, QPointer<cwKeywordItem>> m_items;
};

#endif // CWKEYWORDITEMREGISTRY_H
