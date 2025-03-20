//Our includes
#include "SpyChecker.h"

//Catch includes
#include <catch2/catch_test_macros.hpp>

SpyChecker::SpyChecker()
{

}

SpyChecker::SpyChecker(const std::initializer_list<std::pair<cwSignalSpy *, int> > &list) :
    QHash<cwSignalSpy *, int> (list)
{

}

void SpyChecker::checkSpies() const
{
    for(auto iter = begin(); iter != end(); iter++) {
        INFO("Key:" << iter.key()->objectName().toStdString());
        CHECK(iter.key()->size() == iter.value());
    }
}

void SpyChecker::requireSpies() const
{
    for(auto iter = begin(); iter != end(); iter++) {
        INFO("Key:" << iter.key()->objectName().toStdString());
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
