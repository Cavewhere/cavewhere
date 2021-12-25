#ifndef CWENTITYANDKEYWORDS_H
#define CWENTITYANDKEYWORDS_H

//Qt includes
#include <QPointer>
#include <QObject>
#include <QVector>

//Our includes
#include "cwKeyword.h"

class cwEntityAndKeywords
{
public:
    cwEntityAndKeywords() = default;
    cwEntityAndKeywords(QObject* entity,
                        QVector<cwKeyword>& keywords) :
        mEntity(entity),
        mKeywords(keywords)
    {}

    QObject* entity() const { return mEntity; }
    QVector<cwKeyword> keywords() const { return mKeywords; }
    bool isValid() const { return !mEntity.isNull(); }

    static QVector<QObject*> justEnitites(const QVector<cwEntityAndKeywords>& enitiesKeywords);

private:
    QPointer<QObject> mEntity;
    QVector<cwKeyword> mKeywords;

};

#endif // CWENTITYANDKEYWORDS_H
