#ifndef SPYCHECKER_H
#define SPYCHECKER_H

//Qt includes
#include <QHash>
#include <QSignalSpy>
#include <QMetaMethod>

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


    template<typename Signal>
    QSignalSpy* findSpy(Signal signal) const {
        auto keys = this->keys();
        auto method = QMetaMethod::fromSignal(signal);
        auto iter = std::find_if(keys.begin(), keys.end(), [method](const QSignalSpy* spy) {
            return spy->signal() == method.methodSignature();
        });
        if(iter != keys.end()) {
            return *iter;
        }
        Q_ASSERT(false);
        return nullptr;
    }

    static SpyChecker makeChecker(QObject* object);
};

#endif // SPYCHECKER_H
