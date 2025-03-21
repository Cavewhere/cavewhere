#include "cwDistanceReading.h"
#include "cwDistanceValidator.h"



void cwDistanceReading::updateState() {
    if (m_value.isEmpty()) {
        m_state = State::Empty;
    } else {
        bool ok;
        double value = m_value.toDouble(&ok);
        if(ok && cwDistanceValidator::check(value)) {
            m_state = State::Valid;
        } else {
            m_state = State::Invalid;
        }
    }
}
