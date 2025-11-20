#ifndef CWKEYWORDITEM_H
#define CWKEYWORDITEM_H

//Qt includes
#include <QObject>

//Our includes
#include "cwGlobals.h"
class cwKeywordModel;

class CAVEWHERE_LIB_EXPORT cwKeywordItem : public QObject
{
    Q_OBJECT

    Q_PROPERTY(cwKeywordModel* keywordModel READ keywordModel CONSTANT)
    Q_PROPERTY(QObject* object READ object WRITE setObject NOTIFY objectChanged)

public:
    cwKeywordItem(QObject* parent = nullptr);
    ~cwKeywordItem();

    cwKeywordModel* keywordModel() const;

    QObject* object() const;
    void setObject(QObject* object);

private:
    cwKeywordModel* KeywordModel; //!<
    QObject* Object = nullptr;

signals:
    void objectChanged();

};

/**
* Returns the keywordModel for the item
*/
inline cwKeywordModel* cwKeywordItem::keywordModel() const {
    return KeywordModel;
}

/**
* Returns the object that this keyword extends
*/
inline QObject* cwKeywordItem::object() const {
    return Object;
}

#endif // CWABSTRACTVISIBILITY_H
