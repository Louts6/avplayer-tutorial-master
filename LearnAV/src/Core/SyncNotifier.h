//
// Copyright (c) 2024 Yellow. All rights reserved.
//

#ifndef LEARNAV_SYNCNOTIFIER_H
#define LEARNAV_SYNCNOTIFIER_H

#include <atomic>
#include <condition_variable>
#include <mutex>

namespace av {

class SyncNotifier {
public:
    SyncNotifier() = default;
    ~SyncNotifier() = default;

    void Notify();
    bool Wait(int timeoutInMilliseconds = -1);
    void Reset();

private:
    std::mutex m_mutex;
    std::condition_variable m_cond;
    std::atomic<bool> m_triggered{false};
    bool m_manualReset{false};
};

}  // namespace av

#endif  // LEARNAV_SYNCNOTIFIER_H
