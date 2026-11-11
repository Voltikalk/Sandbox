#pragma once

#include "Types.hpp"
#include <vector>
#include <thread>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <future>
#include <atomic>

namespace sandbox::core {

class ThreadPool {
private:
    std::vector<std::thread> m_workers;
    std::queue<std::function<void()>> m_tasks;

    std::mutex m_queueMutex;
    std::condition_variable m_cvTask;
    std::condition_variable m_cvFinished;

    std::atomic<bool> m_stop{false};
    std::atomic<size_t> m_activeTasks{0};

public:
    explicit ThreadPool(u32 threadCount = 0);
    ~ThreadPool();

    ThreadPool(const ThreadPool&) = delete;
    ThreadPool& operator=(const ThreadPool&) = delete;

    // Добавить произвольную задачу с возвратом std::future
    template <class F, class... Args>
    auto Enqueue(F&& f, Args&&... args)
        -> std::future<typename std::invoke_result<F, Args...>::type> {
        using return_type = typename std::invoke_result<F, Args...>::type;

        auto task = std::make_shared<std::packaged_task<return_type()>>(
            std::bind(std::forward<F>(f), std::forward<Args>(args)...)
        );

        std::future<return_type> res = task->get_future();
        {
            std::unique_lock<std::mutex> lock(m_queueMutex);
            if (m_stop.load()) {
                throw std::runtime_error("Добавление задачи в остановленный ThreadPool");
            }
            m_tasks.emplace([task]() { (*task)(); });
            m_activeTasks.fetch_add(1, std::memory_order_relaxed);
        }
        m_cvTask.notify_one();
        return res;
    }

    // Параллельный цикл для быстрого распределения N задач без лишних аллокаций
    void ParallelFor(size_t count, const std::function<void(size_t)>& loopBody);

    // Дождаться завершения всех активных задач в очереди
    void WaitAll();

    [[nodiscard]] size_t GetWorkerCount() const noexcept { return m_workers.size(); }
};

} // namespace sandbox::core

