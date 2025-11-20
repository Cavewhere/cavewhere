//Our includes
#include "cwKeywordItem.h"
#include "cwKeywordModel.h"

//Qt includes
#include <QDebug>
#include <QPointer>

cwKeywordItem::cwKeywordItem(QObject* parent) :
    QObject(parent),
    KeywordModel(new cwKeywordModel(this))
{

}

cwKeywordItem::~cwKeywordItem()
{
}

/**
* Sets the object for visiblity
*/
void cwKeywordItem::setObject(QObject* object) {
    if(Object != object) {

        if(Object) {
            disconnect(Object, nullptr, this, nullptr);
        }

        Object = object;

        if(Object) {
            QPointer<cwKeywordItem> thiz(this);
            connect(Object, &QObject::destroyed, this, [thiz]() {
                if(thiz) {
                    thiz->Object = nullptr;
                    delete thiz.data();
                }
            });
        }

        emit objectChanged();
    }
}
