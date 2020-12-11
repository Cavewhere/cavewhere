#ifndef CWMINIMIZER_H
#define CWMINIMIZER_H

//Qt includes
#include <QList>

//Our includes
#include "cwGlobals.h"

class CAVEWHERE_LIB_EXPORT cwMinimizer
{
public:
    cwMinimizer() = default;

    void setInitialRange(double min, double max);
    void setIncrement(const QList<double>& increments);

    template <typename Functor>
    void setFunction(Functor f) {
        function = f;
    }

    double findMin() const;

private:
    struct Range {
        double min;
        double max;
    };

    Range InitialRange;
    QList<double> Increments;
    std::function<double (double value)> function;
};

#endif // CWMINIMIZER_H
