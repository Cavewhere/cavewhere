//Our includes
#include "cwKeywordVisibility.h"
#include "cwKeywordItemModel.h"
#include "cwRenderObject.h"

//Qt includes
#include <QLoggingCategory>
#include <QMetaMethod>
#include <QPointer>
#include <QQuickItem>
#include <memory>

Q_LOGGING_CATEGORY(lcKeywordVisibility, "cw.keyword.visibility", QtInfoMsg)

namespace {

bool invokeVisible(QObject* object, bool visible)
{
    if(!object) {
        qCDebug(lcKeywordVisibility) << "invokeVisible: skip (null object) visible=" << visible;
        return false;
    }

    if(auto quickItem = qobject_cast<QQuickItem*>(object)) {
        qCDebug(lcKeywordVisibility) << "invokeVisible: QQuickItem" << static_cast<void*>(object)
                                     << "visible=" << visible;
        quickItem->setVisible(visible);
        return true;
    }

    if(auto renderObject = qobject_cast<cwRenderObject*>(object)) {
        qCDebug(lcKeywordVisibility) << "invokeVisible: cwRenderObject" << static_cast<void*>(object)
                                     << "class=" << object->metaObject()->className()
                                     << "visible=" << visible;
        renderObject->setVisible(visible);
        return true;
    }

    const int methodIndex = object->metaObject()->indexOfMethod("setVisible(bool)");
    if(methodIndex >= 0) {
        qCDebug(lcKeywordVisibility) << "invokeVisible: generic" << static_cast<void*>(object)
                                     << "class=" << object->metaObject()->className()
                                     << "visible=" << visible;
        QMetaObject::invokeMethod(object, "setVisible", Q_ARG(bool, visible));
        return true;
    }

    qCDebug(lcKeywordVisibility) << "invokeVisible: no setVisible(bool) on"
                                 << static_cast<void*>(object)
                                 << "class=" << object->metaObject()->className();
    return false;
}

} // namespace

cwKeywordVisibility::cwKeywordVisibility(QObject *parent) : QObject(parent)
{
}

void cwKeywordVisibility::setVisibleModel(QAbstractItemModel* visibleModel)
{
    if(mVisibleModel == visibleModel) {
        return;
    }

    if(mVisibleModel) {
        disconnect(mVisibleModel, nullptr, this, nullptr);
    }

    mVisibleModel = visibleModel;
    connectModel(mVisibleModel);
    emit visibleModelChanged();
}

void cwKeywordVisibility::setHideModel(QAbstractItemModel* hideModel)
{
    if(mHideModel == hideModel) {
        return;
    }

    if(mHideModel) {
        disconnect(mHideModel, nullptr, this, nullptr);
    }

    mHideModel = hideModel;
    connectModel(mHideModel);
    emit hideModelChanged();
}

bool cwKeywordVisibility::isInModel(QAbstractItemModel* model, QObject* object) const
{
    if(!model || !object) {
        return false;
    }
    const int rows = model->rowCount();
    for(int i = 0; i < rows; ++i) {
        const QModelIndex idx = model->index(i, 0);
        if(idx.data(cwKeywordItemModel::ObjectRole).value<QObject*>() == object) {
            return true;
        }
    }
    return false;
}

void cwKeywordVisibility::resolveVisibility(QObject* object) const
{
    if(!object) {
        return;
    }
    // Hide wins. Without the hide-precedence rule, the same object appearing
    // in both models leaves the final visibility a function of whichever
    // model's rowsInserted fired last.
    if(isInModel(mHideModel, object)) {
        invokeVisible(object, false);
    } else {
        invokeVisible(object, true);
    }
}

void cwKeywordVisibility::connectModel(QAbstractItemModel *model)
{
    if(!model) {
        return;
    }

    // rowsAboutToBeRemoved fires before the rows are gone; stash the affected
    // objects so rowsRemoved (after the model is in its post-remove state) can
    // re-resolve them against the other model.
    auto pendingRemoval = std::make_shared<QList<QPointer<QObject>>>();

    connect(model, &QAbstractItemModel::rowsAboutToBeRemoved,
            this, [model, pendingRemoval](const QModelIndex& parent, int first, int last)
    {
        if(parent.isValid()) {
            return;
        }
        for(int i = first; i <= last; ++i) {
            auto index = model->index(i, 0, parent);
            auto object = index.data(cwKeywordItemModel::ObjectRole).value<QObject*>();
            if(object) {
                pendingRemoval->append(QPointer<QObject>(object));
            }
        }
    });

    connect(model, &QAbstractItemModel::rowsRemoved,
            this, [this, model, pendingRemoval](const QModelIndex& parent, int first, int last)
    {
        if(parent.isValid()) {
            pendingRemoval->clear();
            return;
        }
        qCDebug(lcKeywordVisibility) << "rowsRemoved: model=" << static_cast<void*>(model)
                                     << "rows=" << first << ".." << last
                                     << "captured=" << pendingRemoval->size();
        const auto removed = *pendingRemoval;
        pendingRemoval->clear();
        for(const auto& objPtr : removed) {
            if(objPtr) {
                resolveVisibility(objPtr.data());
            }
        }
    });

    connect(model, &QAbstractItemModel::rowsInserted,
            this, [this, model](const QModelIndex& parent, int first, int last)
    {
        if(parent.isValid()) {
            return;
        }

        qCDebug(lcKeywordVisibility) << "rowsInserted: model=" << static_cast<void*>(model)
                                     << "rows=" << first << ".." << last;
        for(int i = first; i <= last; ++i) {
            auto index = model->index(i, 0, parent);
            auto object = index.data(cwKeywordItemModel::ObjectRole).value<QObject*>();
            resolveVisibility(object);
        }
    });

    connect(model, &QAbstractItemModel::modelReset,
            this, [this, model]()
    {
        qCDebug(lcKeywordVisibility) << "modelReset: model=" << static_cast<void*>(model);
        // Re-resolve every object on each side after a reset; we don't know
        // which entries changed.
        auto walk = [this](QAbstractItemModel* m) {
            if(!m) return;
            const int rows = m->rowCount();
            for(int i = 0; i < rows; ++i) {
                resolveVisibility(m->index(i, 0)
                                  .data(cwKeywordItemModel::ObjectRole)
                                  .value<QObject*>());
            }
        };
        walk(mVisibleModel);
        walk(mHideModel);
    });

    // Initial pass for the rows already present in this model.
    const int rows = model->rowCount();
    for(int i = 0; i < rows; ++i) {
        const QModelIndex index = model->index(i, 0);
        resolveVisibility(index.data(cwKeywordItemModel::ObjectRole).value<QObject*>());
    }
}
