#include "cwCompassReading.h"
#include "cwCompassValidator.h"

void cwCompassReading::updateState() {
    if (m_value.isEmpty()) {
        m_state = Empty;
    } else {
        bool ok;
        double value = m_value.toDouble(&ok);
        if(ok && cwCompassValidator::check(value)) {
            m_state = Valid;
        } else {
            m_state = Invalid;
        }
    }
}
