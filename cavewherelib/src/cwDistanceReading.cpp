#include "cwDistanceReading.h"
#include "cwDistanceValidator.h"



void cwDistanceReading::updateState() {
    QString value = this->value();
    if (value.isEmpty()) {
        m_state = State::Empty;
    } else {
        bool ok;
        double numberValue = value.toDouble(&ok);
        if(ok && cwDistanceValidator::check(numberValue)) {
            m_state = State::Valid;
        } else {
            m_state = State::Invalid;
        }
    }
}
