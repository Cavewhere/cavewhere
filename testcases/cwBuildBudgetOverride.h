#ifndef CWBUILDBUDGETOVERRIDE_H
#define CWBUILDBUDGETOVERRIDE_H

//Qt includes
#include <QByteArray>
#include <QtGlobal>

//! Sets the octree build's memory budget override, the variable the profiling
//! runner sets, for as long as it lives, so a failing check still leaves the
//! environment the way the rest of the suite found it.
class cwBuildBudgetOverride
{
public:
    static constexpr int kDefaultMegabytes = 64;

    explicit cwBuildBudgetOverride(int megabytes = kDefaultMegabytes) :
        m_megabytes(megabytes)
    {
        qputenv(kVariable, QByteArray::number(megabytes));
    }

    ~cwBuildBudgetOverride()
    {
        qunsetenv(kVariable);
    }

    qint64 budgetBytes() const { return qint64(m_megabytes) * kBytesPerMebibyte; }

    cwBuildBudgetOverride(const cwBuildBudgetOverride&) = delete;
    cwBuildBudgetOverride& operator=(const cwBuildBudgetOverride&) = delete;

private:
    static constexpr const char* kVariable = "CW_BUILD_MEMORY_BUDGET_MB";
    static constexpr qint64 kBytesPerMebibyte = 1024 * 1024;

    int m_megabytes;
};

#endif // CWBUILDBUDGETOVERRIDE_H
