#include "cwDistanceReading.h"
#include "cwDistanceValidator.h"



void cwDistanceReading::updateState() {
    QString value = this->value();
    if (value.isEmpty()) {
        setState(static_cast<int>(State::Empty));
    } else {
        bool ok;
        double numberValue = value.toDouble(&ok);
        if(ok && cwDistanceValidator::check(numberValue)) {
            setState(static_cast<int>(State::Valid));
        } else {
            setState(static_cast<int>(State::Invalid));;
        }
    }
}
