#include "ThreadPool.hpp"
#include "Log.hpp"
#include <algorithm>

namespace sandbox::core {

ThreadPool::ThreadPool(u32 threadCount) {
    if (threadCount == 0) {
        threadCount = std::thread::hardware_concurrency();
        if (threadCount == 0) threadCount = 4;
    }

    LOG_INFO("ThreadPool", "Инициализация пула потоков с ", threadCount, " рабочими потоками");

    m_workers.reserve(threadCount);
    for (u32 i = 0; i < threadCount; ++i) {
        m_workers.emplace_back([this, i]() {
            while (true) {
                std::function<void()> task;
                {
                    std::unique_lock<std::mutex> lock(this->m_queueMutex);
                    this->m_cvTask.wait(lock, [this]() {
                        return this->m_stop.load() || !this->m_tasks.empty();
                    });

                    if (this->m_stop.load() && this->m_tasks.empty()) {
                        return;
                    }

                    task = std::move(this->m_tasks.front());
                    this->m_tasks.pop();
                }

                try {
                    task();
                } catch (const std::exception& e) {
                    LOG_ERROR("ThreadPool", "Исключение в рабочем потоке ", i, ": ", e.what());
                } catch (...) {
                    LOG_ERROR("ThreadPool", "Неизвестное исключение в рабочем потоке ", i);
                }

                if (this->m_activeTasks.fetch_sub(1, std::memory_order_acq_rel) == 1) {
                    this->m_cvFinished.notify_all();
                }
            }
        });
    }
}

ThreadPool::~ThreadPool() {
    m_stop.store(true, std::memory_order_release);
    m_cvTask.notify_all();

    for (std::thread& worker : m_workers) {
        if (worker.joinable()) {
            worker.join();
        }
    }
    LOG_INFO("ThreadPool", "Пул потоков успешно остановлен.");
}

void ThreadPool::WaitAll() {
    std::unique_lock<std::mutex> lock(m_queueMutex);
    m_cvFinished.wait(lock, [this]() {
        return m_tasks.empty() && m_activeTasks.load(std::memory_order_acquire) == 0;
    });
}

void ThreadPool::ParallelFor(size_t count, const std::function<void(size_t)>& loopBody) {
    if (count == 0) return;

    size_t workerCount = m_workers.size();
    if (workerCount <= 1 || count == 1) {
        // Однопоточное выполнение если пул с 1 потоком или 1 элемент
        for (size_t i = 0; i < count; ++i) {
            loopBody(i);
        }
        return;
    }

    // Распределяем элементы блоками по количеству воркеров
    size_t batchSize = (count + workerCount - 1) / workerCount;
    std::atomic<size_t> remainingBatches{0};

    size_t start = 0;
    while (start < count) {
        size_t end = std::min(start + batchSize, count);
        remainingBatches.fetch_add(1, std::memory_order_relaxed);

        Enqueue([start, end, &loopBody, &remainingBatches]() {
            for (size_t i = start; i < end; ++i) {
                loopBody(i);
            }
            remainingBatches.fetch_sub(1, std::memory_order_release);
        });

        start = end;
    }

    // Ожидаем завершения всех батчей
    while (remainingBatches.load(std::memory_order_acquire) > 0) {
        std::this_thread::yield();
    }
}

} // namespace sandbox::core

