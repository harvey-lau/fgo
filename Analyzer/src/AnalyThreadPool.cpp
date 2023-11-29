/**
 *
 *
 */

#include "AnalyThreadPool.h"

namespace FGo
{
namespace Analy
{
void ThreadPool::ThreadWorker::operator()()
{
    std::function<void()> func; // Define function class "func"
    bool isDequeued;            // Whether the element is dequeued
    while (!m_pool->m_shutFlag) {
        {
            // Load mutex
            std::unique_lock<std::mutex> lock(m_pool->m_conditionMutex);

            // If the task queue is empty, block the current thread
            if (m_pool->m_queue.empty()) {
                // Wait for conditional variable and start thread
                m_pool->m_conditionLock.wait(lock);
            }
            // Pop out the element
            isDequeued = m_pool->m_queue.dequeue(func);
        }

        // If the function is dequeued successfully, execute the function
        if (isDequeued) func();
    }
}

ThreadPool::ThreadPool()
{
    unsigned threadCount = std::thread::hardware_concurrency();
    if (threadCount == 0) threadCount = 4;
    m_threads = std::vector<std::thread>(threadCount);
    m_shutFlag = false;
}

ThreadPool::ThreadPool(unsigned threadCount)
{
    m_threads = std::vector<std::thread>(threadCount);
    m_shutFlag = false;
}

void ThreadPool::init()
{
    for (size_t i = 0; i < m_threads.size(); ++i) {
        // Assign a worker thread
        m_threads.at(i) = std::thread(ThreadWorker(this, i));
    }
}

void ThreadPool::init(unsigned threadCount)
{
    m_threads.resize(threadCount);
    m_shutFlag = false;
    for (size_t i = 0; i < m_threads.size(); ++i) {
        // Assign a worker thread
        m_threads.at(i) = std::thread(ThreadWorker(this, i));
    }
}

void ThreadPool::shutdown()
{
    m_shutFlag = true;
    m_conditionLock.notify_all(); // wake up all the threads
    for (size_t i = 0; i < m_threads.size(); ++i) {
        if (m_threads.at(i).joinable()) {
            m_threads.at(i).join();
        }
    }
}
} // namespace Analy
} // namespace FGo