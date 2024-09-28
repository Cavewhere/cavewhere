#ifndef SPYCHECKER_H
#define SPYCHECKER_H

//Qt includes
#include <QHash>
#include "cwSignalSpy.h"

//Std includes
#include <initializer_list>

class SpyChecker : public QHash<cwSignalSpy*, int>
{
public:
    SpyChecker();
    SpyChecker(const std::initializer_list<std::pair<cwSignalSpy*, int>>& list);

    void checkSpies() const;
    void requireSpies() const;
    void clearSpyCounts();
};

#endif // SPYCHECKER_H
