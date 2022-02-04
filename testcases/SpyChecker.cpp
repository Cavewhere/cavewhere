//Our includes
#include "SpyChecker.h"

//Catch includes
#include "catch.hpp"

//Qt includes
#include <QDebug>

SpyChecker::SpyChecker()
{

}

SpyChecker::SpyChecker(const std::initializer_list<std::pair<QSignalSpy *, int> > &list) :
    QHash<QSignalSpy *, int> (list)
{

}

void SpyChecker::checkSpies() const
{
    for(auto iter = begin(); iter != end(); iter++) {
        INFO("Key:" << iter.key()->objectName().toStdString());
        if(iter.key()->size() != iter.value()) {
            qDebug() << "Spy checker will fail. Place breakpoint here to debug. checkSpies()";
        }
        INFO("SignalSpy:" << iter.key()->size() << " expected:" << iter.value());
        CHECK(iter.key()->size() == iter.value());
    }
}

void SpyChecker::requireSpies() const
{
    for(auto iter = begin(); iter != end(); iter++) {
        INFO("Key:" << iter.key()->objectName().toStdString())
        if(iter.key()->size() != iter.value()) {
            qDebug() << "Spy checker will fail. Place breakpoint here to debug. requireSpies()";
        }
        REQUIRE(iter.key()->size() == iter.value());
    }
}

void SpyChecker::clearSpyCounts()
{
    for(auto iter = begin(); iter != end(); iter++) {
        iter.key()->clear();
        iter.value() = 0;
    }
}

SpyChecker SpyChecker::makeChecker(QObject *object)
{
    SpyChecker checker;

    auto metaObject = object->metaObject();
    for(int i = 0; i < metaObject->methodCount(); i++) {
        auto method = metaObject->method(i);
        if(method.methodType() == QMetaMethod::Signal) {
            QSignalSpy* spy = new QSignalSpy(object, method);
            spy->setObjectName(method.name() + "Spy");
            checker.insert(spy, 0);
        }
    }

    return checker;
}
