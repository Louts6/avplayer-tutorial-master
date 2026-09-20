//
// Copyright (c) 2024 Yellow. All rights reserved.
//

#include "TaskPool.h"

namespace av {

TaskPool::TaskPool() {
    m_thread = std::thread([this]() { this->ThreadLoop(); });
}

TaskPool::~TaskPool() {
    // 终止任务并等待线程退出
    {
        std::unique_lock<std::mutex> lock(m_taskMutex);
        m_stopFlag = true;
    }
    m_taskCondition.notify_all();
    if (m_thread.joinable()) {
        m_thread.join();
    }
}

void TaskPool::SubmitTask(std::function<void()> task) {
    {
        std::unique_lock<std::mutex> lock(m_taskMutex);
        m_tasks.push(task);
    }
    m_taskCondition.notify_one();
}

void TaskPool::ThreadLoop() {
    while (true) {
        std::function<void()> task;
        {
            std::unique_lock<std::mutex> lock(m_taskMutex);
            m_taskCondition.wait(lock, [this]() { return !m_tasks.empty() || m_stopFlag; });

            if (m_stopFlag && m_tasks.empty()) {
                break;
            }

            task = m_tasks.front();
            m_tasks.pop();
        }
        task();
    }
}

}  // namespace av