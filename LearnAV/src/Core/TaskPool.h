//
// Copyright (c) 2024 Yellow. All rights reserved.
//

#ifndef LEARNAV_TASKPOOL_H
#define LEARNAV_TASKPOOL_H

#include <atomic>
#include <condition_variable>
#include <functional>
#include <mutex>
#include <queue>
#include <thread>

namespace av {

class TaskPool {
public:
    TaskPool();
    ~TaskPool();
    void SubmitTask(std::function<void()> task);

private:
    void ThreadLoop();

private:
    std::thread m_thread;
    std::queue<std::function<void()>> m_tasks;
    std::condition_variable m_taskCondition;
    std::mutex m_taskMutex;
    std::atomic<bool> m_stopFlag{false};
};

}  // namespace av

#endif  // LEARNAV_TASKPOOL_H
