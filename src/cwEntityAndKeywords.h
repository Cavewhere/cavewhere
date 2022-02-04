#ifndef CWENTITYANDKEYWORDS_H
#define CWENTITYANDKEYWORDS_H

//Qt includes
#include <QPointer>
#include <QObject>
#include <QVector>
#include <QVariant>

//Our includes
#include "cwKeyword.h"

class cwEntityAndKeywords
{
public:
    cwEntityAndKeywords() = default;
    cwEntityAndKeywords(QObject* entity,
                        const QVector<cwKeyword>& keywords) :
        mEntity(entity),
        mKeywords(keywords)
    {}

    QObject* entity() const { return mEntity; }
    QVector<cwKeyword> keywords() const { return mKeywords; }
    bool isValid() const { return mEntity != nullptr; }

    static QVector<QObject*> justEnitites(const QVector<cwEntityAndKeywords>& enitiesKeywords);

private:
    QObject* mEntity = nullptr;
    QVector<cwKeyword> mKeywords;

};

Q_DECLARE_METATYPE(cwEntityAndKeywords)

#endif // CWENTITYANDKEYWORDS_H
