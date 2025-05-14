#include "cwClinoReading.h"
#include "cwClinoValidator.h"

void cwClinoReading::updateState() {
    const QString value = this->value();
    if (value.isEmpty()) {
        setState(static_cast<int>(State::Empty));
    } else if (value.compare("down", Qt::CaseInsensitive) == 0) {
        setState(static_cast<int>(State::Down));
    } else if (value.compare("up", Qt::CaseInsensitive) == 0) {
        setState(static_cast<int>(State::Up));
    } else {
        bool ok;
        double numberValue = value.toDouble(&ok);
        if(ok && cwClinoValidator::check(numberValue)) {
            setState(static_cast<int>(State::Valid));
        } else {
            setState(static_cast<int>(State::Invalid));
        }
    }
}
