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
 * Created by Joshua Yao on Mar. 27, 2023
 *
 */

#ifndef JY_THREADPOOL_H_
#define JY_THREADPOOL_H_

#include <condition_variable>
#include <functional>
#include <future>
#include <mutex>
#include <queue>
#include <thread>

namespace FGo
{
namespace Analy
{

template <typename T>
class SafeQueue
{
private:
    std::queue<T> m_queue;
    std::mutex m_mutex;

public:
    SafeQueue()
    {}
    SafeQueue(SafeQueue &&other)
    {}
    ~SafeQueue() = default;
    bool empty()
    {
        std::unique_lock<std::mutex> lock(m_mutex);
        return m_queue.empty();
    }
    size_t size()
    {
        std::unique_lock<std::mutex> lock(m_mutex);
        return (size_t)m_queue.size();
    }
    void enqueue(T &t)
    {
        std::unique_lock<std::mutex> lock(m_mutex);
        m_queue.emplace(t);
    }
    bool dequeue(T &t)
    {
        std::unique_lock<std::mutex> lock(m_mutex);
        if (m_queue.empty()) return false;
        t = std::move(m_queue.front());
        m_queue.pop();
        return true;
    }
};

class ThreadPool
{
private:
    class ThreadWorker
    {
    private:
        int m_id;           // Thread pool worker id
        ThreadPool *m_pool; // The threadpool that the thread worker belongs to

    public:
        ThreadWorker(ThreadPool *pool, const int id) : m_id(id), m_pool(pool)
        {}
        void operator()();
    };

    bool m_shutFlag;                          // Whether the threadpool is closed
    SafeQueue<std::function<void()>> m_queue; // Task queue
    std::vector<std::thread> m_threads;       // Worker threads
    std::mutex m_conditionMutex;              // Mutex for thread sleeping
    std::condition_variable m_conditionLock;  // Mutex for thread condition

public:
    /// @brief Constructor with the deafult count of threads
    /// equal to the count of CPU cores.
    ThreadPool();

    /// @brief Counstructor with `threadCount` threads.
    /// @param threadCount
    ThreadPool(unsigned threadCount);

    ThreadPool(const ThreadPool &) = delete;
    ThreadPool(ThreadPool &&) = delete;

    ThreadPool &operator=(const ThreadPool &) = delete;
    ThreadPool &operator=(ThreadPool &&) = delete;

    /// @brief Initialize this thread pool.
    void init();

    /// @brief Initialize this thread pool with `threadCount` threads.
    /// @param threadCount
    void init(unsigned threadCount);

    /// @brief Wait until the threads finish their current tasks and shutdown the pool.
    void shutdown();

    // Submit a function to be executed asynchronously by the pool
    template <typename F, typename... Args>
    auto submit(F &&f, Args &&...args) -> std::future<decltype(f(args...))>
    {
        // Create a function with bounded parameter ready to execute
        std::function<decltype(f(args...))()> func =
            std::bind(std::forward<F>(f), std::forward<Args>(args)...);

        // Encapsulate it into a shared pointer in order to be able to copy constructor
        auto taskPointer = std::make_shared<std::packaged_task<decltype(f(args...))()>>(func);

        // Warp packaged task into void function
        std::function<void()> wrapperFunc = [taskPointer]() {
            (*taskPointer)();
        };

        // Push the function into safe queue
        m_queue.enqueue(wrapperFunc);

        // Wake up a waiting thread
        m_conditionLock.notify_one();

        // Return the registered pointer
        return taskPointer->get_future();
    }
};
} // namespace Analy
} // namespace FGo

#endif