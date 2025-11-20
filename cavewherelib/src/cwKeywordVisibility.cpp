//Our includes
#include "cwKeywordVisibility.h"
#include "cwKeywordItemModel.h"
#include "cwRenderObject.h"

//Qt includes
#include <QQuickItem>
#include <QMetaMethod>

namespace {

bool invokeVisible(QObject* object, bool visible)
{
    if(!object) {
        return false;
    }

    if(auto quickItem = qobject_cast<QQuickItem*>(object)) {
        quickItem->setVisible(visible);
        return true;
    }

    if(auto renderObject = qobject_cast<cwRenderObject*>(object)) {
        renderObject->setVisible(visible);
        return true;
    }

    const int methodIndex = object->metaObject()->indexOfMethod("setVisible(bool)");
    if(methodIndex >= 0) {
        QMetaObject::invokeMethod(object, "setVisible", Q_ARG(bool, visible));
        return true;
    }

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
    connectModel(mVisibleModel, true);
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
    connectModel(mHideModel, false);
    emit hideModelChanged();
}

void cwKeywordVisibility::applyVisibility(QAbstractItemModel *model, bool visible) const
{
    if(!model) {
        return;
    }

    const int rows = model->rowCount();
    for(int i = 0; i < rows; ++i) {
        auto index = model->index(i, 0);
        auto object = index.data(cwKeywordItemModel::ObjectRole).value<QObject*>();
        invokeVisible(object, visible);
    }
}

void cwKeywordVisibility::connectModel(QAbstractItemModel *model, bool visible)
{
    if(!model) {
        return;
    }

    connect(model, &QAbstractItemModel::rowsInserted,
            this, [model, visible](const QModelIndex& parent, int first, int last)
    {
        if(parent.isValid()) {
            return;
        }

        for(int i = first; i <= last; ++i) {
            auto index = model->index(i, 0, parent);
            auto object = index.data(cwKeywordItemModel::ObjectRole).value<QObject*>();
            invokeVisible(object, visible);
        }
    });

    connect(model, &QAbstractItemModel::modelReset,
            this, [this, model, visible]()
    {
        applyVisibility(model, visible);
    });

    applyVisibility(model, visible);
}
