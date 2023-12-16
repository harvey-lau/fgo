/**
 *

    MIT License

    Copyright (c) 2016 Mariano Trebino

    Permission is hereby granted, free of charge, to any person obtaining a copy
    of this software and associated documentation files (the "Software"), to deal
    in the Software without restriction, including without limitation the rights
    to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
    copies of the Software, and to permit persons to whom the Software is
    furnished to do so, subject to the following conditions:

    The above copyright notice and this permission notice shall be included in all
    copies or substantial portions of the Software.

    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
    IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
    FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
    AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
    LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
    OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
    SOFTWARE.

    From https://github.com/mtrebi/thread-pool

*/

/**
 * Created by Joshua Yao on Nov. 16, 2023
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