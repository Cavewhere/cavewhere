#include "cwCompassReading.h"
#include "cwCompassValidator.h"

void cwCompassReading::updateState() {
    auto str = value();
    if (str.isEmpty()) {
        setState(static_cast<int>(State::Empty));
    } else {
        bool ok;
        double value = str.toDouble(&ok);
        if(ok && cwCompassValidator::check(value)) {
            setState(static_cast<int>(State::Valid));
        } else {
            setState(static_cast<int>(State::Invalid));
        }
    }
}
