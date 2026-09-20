//
// Copyright (c) 2024 Yellow. All rights reserved.
//

#include "SyncNotifier.h"

namespace av {

void SyncNotifier::Notify() {
    std::unique_lock<std::mutex> lock(m_mutex);
    m_triggered = true;
    m_cond.notify_all();
}

bool SyncNotifier::Wait(int timeOutMilliseconds) {
    std::unique_lock<std::mutex> lock(m_mutex);

    if (!m_triggered) {
        if (timeOutMilliseconds < 0) {
            m_cond.wait(lock, [this] { return m_triggered.load(); });
        } else {
            if (!m_cond.wait_for(lock, std::chrono::milliseconds(timeOutMilliseconds),
                                 [this] { return m_triggered.load(); })) {
                return false;
            }
        }
    }

    Reset();

    return true;
}

void SyncNotifier::Reset() { m_triggered = false; }

}  // namespace av
