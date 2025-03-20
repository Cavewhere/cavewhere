#include "cwMinimizer.h"

//Qt includes
#include <QDebug>

void cwMinimizer::setInitialRange(double min, double max)
{
    InitialRange = {min, max};
}

void cwMinimizer::setIncrement(const QList<double> &increments)
{
    Increments = increments;
}

double cwMinimizer::findMin() const
{
    struct SearchArea {
        double left;
        double min;
        double right;
    };

    auto findLocalMin = [this](SearchArea area, double increment){
        Q_ASSERT(area.right > area.left);
        int numberOfSteps = static_cast<int>((area.right - area.left) / increment) + 1;

        double minOutput = function(area.left);
        double minInput = area.left;

        for(int i = 1; i < numberOfSteps; i++) {
            double inputValue = i * increment + area.left;
            double outputValue = function(inputValue);
            if(outputValue < minOutput) {
                minOutput = outputValue;
                minInput = inputValue;
            }
        }

        return SearchArea({std::max(minInput - increment, area.left),  //Left
                           minInput, //Min ie the middle
                           std::min(minInput + increment, area.right)}); //Right
    };

    SearchArea currentArea{InitialRange.min, 0.0, InitialRange.max};
    for(auto increment : Increments) {
        currentArea = findLocalMin(currentArea, increment);
    }

    return currentArea.min;
}
