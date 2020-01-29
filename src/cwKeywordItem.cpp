//Our includes
#include "cwKeywordItem.h"
#include "cwKeywordModel.h"

cwKeywordItem::cwKeywordItem(QObject* parent) :
    QObject(parent),
    KeywordModel(new cwKeywordModel(this))
{

}

/**
* Sets the object for visiblity
*/
void cwKeywordItem::setObject(QObject* object) {
    if(Object != object) {
        Object = object;
        setParent(Object);
        emit objectChanged();
    }
}
