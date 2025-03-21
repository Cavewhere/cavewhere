#include "cwClinoReading.h"
#include "cwClinoValidator.h"

void cwClinoReading::updateState() {
    if (m_value.isEmpty()) {
        m_state = Empty;
    } else if (m_value.compare("down", Qt::CaseInsensitive) == 0) {
        m_state = Down;
    } else if (m_value.compare("up", Qt::CaseInsensitive) == 0) {
        m_state = Up;
    } else {
        bool ok;
        double value = m_value.toDouble(&ok);
        if(ok && cwClinoValidator::check(value)) {
            m_state = Valid;
        } else {
            m_state = Invalid;
        }
    }
}
