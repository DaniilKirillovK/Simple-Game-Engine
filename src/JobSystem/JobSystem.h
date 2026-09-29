#pragma once

#include <atomic>
#include <cstdint>
#include <vector>
#include <thread>
#include <memory>
#include <mutex>
#include <condition_variable>
#include <chrono>
#include <algorithm>
#include <cassert>

constexpr size_t CACHE_LINE_SIZE = 64;

template<typename T, size_t Capacity>
class WorkStealingDeque 
{
public:
    WorkStealingDeque() = default;
    WorkStealingDeque(const WorkStealingDeque&) = delete;
    WorkStealingDeque& operator=(const WorkStealingDeque&) = delete;

    bool push(T item) 
    {
        const int64_t bottom = m_bottom.load();
        const int64_t top = m_top.load();

        if (bottom - top >= static_cast<int64_t>(Capacity)) return false;

        m_buffer[bottom % Capacity] = item;
        m_bottom.store(bottom + 1);

        return true;
    }

    bool pop(T& out) 
    {
        int64_t bottom = m_bottom.load() - 1;
        m_bottom.store(bottom);
        const int64_t top = m_top.load();

        if (top <= bottom)
        {
            out = m_buffer[bottom % Capacity];
            if (top == bottom)
            {
                if (!m_top.compare_exchange_strong(bottom, bottom + 1)) 
                {
                    m_bottom.store(bottom + 1);
                    return false;
                }
                m_bottom.store(bottom + 1);
            }
            return true;
        }

        m_bottom.store(bottom + 1);
        return false;
    }

    bool steal(T& out) 
    {
        int64_t top = m_top.load();
        const int64_t bottom = m_bottom.load();
        if (top < bottom)
        {
            out = m_buffer[top % Capacity];
            if (!m_top.compare_exchange_strong(top, top + 1))
            {
                return false;
            }
            return true;
        }
        return false;
    }

    bool empty() const 
    {
        return m_bottom.load() <= m_top.load();
    }

private:
    alignas(CACHE_LINE_SIZE) std::atomic<int64_t> m_top { 0 };
    alignas(CACHE_LINE_SIZE) std::atomic<int64_t> m_bottom { 0 };
    alignas(CACHE_LINE_SIZE) T m_buffer[Capacity];
};

class JobCounter 
{
    friend class JobSystem;
public:
    JobCounter() : m_count(1) {}

    bool isDone() const 
    {
        return m_count.load() == 0;
    }

private:
    std::atomic<int32_t> m_count;
};

using Function = void(*)(void*, uint32_t);

struct Job
{
    Function fn = nullptr;
    void* data = nullptr;
    uint32_t start = 0;
    uint32_t end = 0;
    std::atomic<int32_t>* counter = nullptr;
    Job* next = nullptr;
};

class JobSystem 
{
public:
    explicit JobSystem(uint32_t numWorkers = 0);
    ~JobSystem();

    JobSystem(const JobSystem&) = delete;
    JobSystem& operator=(const JobSystem&) = delete;

    void execute(Function fn, void* data);

    void parallelFor(uint32_t count, uint32_t batch, Function fn, void* data);

    void parallelForAsync(uint32_t count, uint32_t batch,
        Function fn, void* data, JobCounter* counter);

    void wait(JobCounter* counter);

    uint32_t getNumWorkers() const { return m_numWorkers; }

private:
    static constexpr uint32_t DEQUE_CAPACITY = 4096;
    static constexpr uint32_t POOL_CHUNK_SIZE = 4096;

    struct alignas(CACHE_LINE_SIZE) Worker 
    {
        WorkStealingDeque<Job*, DEQUE_CAPACITY> m_deque;
    };

    void workerLoop(uint32_t id);
    Job* allocateJob();
    void freeJob(Job* job);
    void submit(Job* job, uint32_t preferred);
    bool tryPopJob(uint32_t id, Job*& out);
    bool tryStealJob(uint32_t skipId, Job*& out);
    void executeJob(Job* job);

    static thread_local uint32_t m_tlsWorkerIndex;

    std::vector<std::thread> m_threads;
    std::vector<std::unique_ptr<Worker>> m_workers;
    std::atomic<bool> m_stop{ false };
    uint32_t m_numWorkers = 0;

    std::mutex m_poolMutex;
    Job* m_freeList = nullptr;
    std::vector<std::unique_ptr<Job[]>> m_poolChunks;

    std::condition_variable m_cv;
    std::mutex m_cvMutex;
};

