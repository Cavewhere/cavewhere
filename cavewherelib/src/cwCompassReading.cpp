#include "cwCompassReading.h"
#include "cwCompassValidator.h"

void cwCompassReading::updateState() {
    auto str = value();
    if (str.isEmpty()) {
        m_state = State::Empty;
    } else {
        bool ok;
        double value = str.toDouble(&ok);
        if(ok && cwCompassValidator::check(value)) {
            m_state = State::Valid;
        } else {
            m_state = State::Invalid;
        }
    }
}
