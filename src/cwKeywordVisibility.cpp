//Our includes
#include "cwKeywordVisibility.h"
#include "cwKeywordItemModel.h"

//Qt includes
#include <Qt3DCore/QEntity>
#include <QQuickItem>

cwKeywordVisibility::cwKeywordVisibility(QObject *parent) : QObject(parent)
{

}

void cwKeywordVisibility::setVisibleModel(QAbstractItemModel* visibleModel) {
    if(mVisibleModel != visibleModel) {
        if(mVisibleModel) {
            disconnect(mVisibleModel, nullptr, this, nullptr);
        }

        mVisibleModel = visibleModel;

        connectModel(mVisibleModel, true);

        emit visibleModelChanged();
    }
}


void cwKeywordVisibility::setHideModel(QAbstractItemModel* hideModel) {
    if(mHideModel != hideModel) {
        if(mHideModel) {
            disconnect(mHideModel, nullptr, this, nullptr);
        }

        mHideModel = hideModel;

        connectModel(mHideModel, false);

        emit hideModelChanged();
    }
}

void cwKeywordVisibility::connectModel(QAbstractItemModel *model, bool visible)
{
    if(model) {
        auto setVisible = [visible](QObject* object) {
            if(auto quickItem = dynamic_cast<QQuickItem*>(object)) {
                quickItem->setVisible(visible);
            } else if(auto qt3dEntity = dynamic_cast<Qt3DCore::QEntity*>(object)) {
                qt3dEntity->setEnabled(visible);
            }
        };

        connect(model, &QAbstractItemModel::rowsInserted,
                this, [setVisible, model](const QModelIndex& parent, int first, int last)
        {
            if(parent == QModelIndex()) {
                for(int i = first; i <= last; i++) {
                    auto index = model->index(i, 0, parent);
                    setVisible(index.data(cwKeywordItemModel::ObjectRole).value<QObject*>());
                }
            }
        });
    }
}
