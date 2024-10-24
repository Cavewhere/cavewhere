#ifndef TRACKED_H
#define TRACKED_H

template <typename T>
class cwTracked {
public:
    cwTracked() {}

    cwTracked(const T& initialValue)
        : m_value(initialValue), m_needsUpdate(false) {}

    // Getter for the value
    const T& value() const {
        return m_value;
    }

    // Setter for the value, which marks it as needing an update
    void setValue(const T& newValue) {
        if (m_value != newValue) {  // Only mark as updated if the value is changed
            m_value = newValue;
            m_needsUpdate = true;   // Mark as needing update
        }
    }

    // Check if the data needs to be updated
    bool isChanged() const {
        return m_needsUpdate;
    }

    // Mark the data as updated (typically after the renderer processes the update)
    void resetChanged() {
        m_needsUpdate = false;
    }

private:
    T m_value;         // The actual data
    bool m_needsUpdate = false; // Tracks whether the data needs to be updated
};


#endif // TRACKED_H
