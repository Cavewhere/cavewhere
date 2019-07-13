#ifndef SPYCHECKER_H
#define SPYCHECKER_H

//Qt includes
#include <QHash>
#include <QSignalSpy>

//Std includes
#include <initializer_list>

class SpyChecker : public QHash<QSignalSpy*, int>
{
public:
    SpyChecker();
    SpyChecker(const std::initializer_list<std::pair<QSignalSpy*, int>>& list);

    void checkSpies() const;
    void requireSpies() const;
    void clearSpyCounts();
};

#endif // SPYCHECKER_H
