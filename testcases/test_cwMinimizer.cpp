//Catch includes
#include <catch2/catch_test_macros.hpp>

//Our includes
#include "cwMinimizer.h"

//Qt includes
#include <QDebug>

TEST_CASE("Minimizer should find the min value", "[cwMinimizer]") {

    cwMinimizer minimizer;
    minimizer.setIncrement({10, 1, 0.1, 0.01, 0.001});
    minimizer.setInitialRange(-40, 100);

    auto function = [](double x) {
        double z = 0.2 * x - 10.1742;
        return z * z + 10.0;
    };

    //Min value should be x = 50.5 with min value of y = 10;
    minimizer.setFunction(function);

    CHECK(minimizer.findMin() == 50.871);
    CHECK(function(50.871) == 10.0);

}
