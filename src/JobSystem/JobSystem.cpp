#include "JobSystem/JobSystem.h"
#include "tracy/Tracy.hpp"

thread_local uint32_t JobSystem::m_tlsWorkerIndex = UINT32_MAX;

JobSystem::JobSystem(uint32_t numWorkers)
{
    if (numWorkers == 0) 
    {
        numWorkers = std::thread::hardware_concurrency();
        if (numWorkers == 0) numWorkers = 4;
    }
    m_numWorkers = numWorkers;

    m_workers.reserve(m_numWorkers);
    for (uint32_t i = 0; i < m_numWorkers; ++i)
    {
        m_workers.push_back(std::make_unique<Worker>());
    }
     
    auto chunk = std::make_unique<Job[]>(POOL_CHUNK_SIZE);
    for (uint32_t i = 0; i < POOL_CHUNK_SIZE; ++i) 
    {
        chunk[i].next = m_freeList;
        m_freeList = &chunk[i];
    }
    m_poolChunks.push_back(std::move(chunk));

    for (uint32_t i = 0; i < m_numWorkers; ++i)
    {
        m_threads.emplace_back([this, i] { workerLoop(i); });
    }
}

JobSystem::~JobSystem()
{
    m_stop.store(true);
    m_cv.notify_all();
    for (auto& t : m_threads)
    {
        if (t.joinable()) t.join();
    }
}

void JobSystem::execute(Function fn, void* data)
{
    Job* job = allocateJob();
    job->fn = fn;
    job->data = data;
    job->start = 0;
    job->end = 1;
    job->counter = nullptr;
    submit(job, 0);
}

void JobSystem::parallelFor(uint32_t count, uint32_t batch, Function fn, void* data)
{
    if (count == 0) return;
    if (batch == 0) batch = 1;

    JobCounter counter;
    const uint32_t numBatches = (count + batch - 1) / batch;
    counter.m_count.store(static_cast<int32_t>(numBatches));

    for (uint32_t b = 0; b < numBatches; ++b) 
    {
        const uint32_t start = b * batch;
        const uint32_t end = std::min(start + batch, count);

        Job* job = allocateJob();
        job->fn = fn;
        job->data = data;
        job->start = start;
        job->end = end;
        job->counter = &counter.m_count;
        submit(job, b % m_numWorkers);
    }

    wait(&counter);
}

void JobSystem::parallelForAsync(uint32_t count, uint32_t batch, Function fn, void* data, JobCounter* counter)
{
    if (count == 0) 
    {
        counter->m_count.store(0);
        return;
    }
    if (batch == 0) batch = 1;

    const uint32_t numBatches = (count + batch - 1) / batch;
    counter->m_count.store(static_cast<int32_t>(numBatches));

    for (uint32_t b = 0; b < numBatches; ++b) 
    {
        const uint32_t start = b * batch;
        const uint32_t end = std::min(start + batch, count);

        Job* job = allocateJob();
        job->fn = fn;
        job->data = data;
        job->start = start;
        job->end = end;
        job->counter = &counter->m_count;
        submit(job, b % m_numWorkers);
    }
}

void JobSystem::wait(JobCounter* counter)
{
    ZoneScopedN("JobSystem::Wait");

    const uint32_t myIdx = m_tlsWorkerIndex;

    while (counter->m_count.load() > 0) 
    {
        Job* job = nullptr;
        bool didWork = false;

        // Own deque first — our children are usually here.
        if (myIdx != UINT32_MAX && tryPopJob(myIdx, job)) 
        {
            executeJob(job);
            didWork = true;
        }

        // Then steal from others.
        if (!didWork) 
        {
            for (uint32_t i = 0; i < m_numWorkers; ++i) 
            {
                if (i == myIdx) continue;
                if (m_workers[i]->m_deque.steal(job))
                {
                    executeJob(job);
                    didWork = true;
                    break;
                }
            }
        }

        if (!didWork)
        {
            std::this_thread::yield();
        }
    }
}

void JobSystem::workerLoop(uint32_t id)
{
    char threadName[32];
    snprintf(threadName, sizeof(threadName), "Worker %u", id);
    tracy::SetThreadName(threadName);

    m_tlsWorkerIndex = id;

    while (!m_stop.load()) 
    {
        Job* job = nullptr;

        {
            ZoneScopedN("Worker::PopLocal");
            // 1. Try our own deque
            if (tryPopJob(id, job))
            {
                executeJob(job);
                continue;
            }
        }

        {
            ZoneScopedN("Worker::Steal");
            // 2. Try to steal from someone else.
            if (tryStealJob(id, job))
            {
                executeJob(job);
                continue;
            }
        }

        {
            ZoneScopedN("Worker::Idle");
            // 3. Nothing to do — wait briefly.
            std::unique_lock<std::mutex> lock(m_cvMutex);
            m_cv.wait_for(lock, std::chrono::microseconds(100), [this, id] {
                return m_stop.load() ||
                    !m_workers[id]->m_deque.empty();
                }
            );
        }
    }
}

Job* JobSystem::allocateJob()
{
    std::lock_guard<std::mutex> lock(m_poolMutex);
    if (!m_freeList) 
    {
        auto chunk = std::make_unique<Job[]>(POOL_CHUNK_SIZE);
        for (uint32_t i = 0; i < POOL_CHUNK_SIZE; ++i) 
        {
            chunk[i].next = m_freeList;
            m_freeList = &chunk[i];
        }
        m_poolChunks.push_back(std::move(chunk));
    }

    Job* job = m_freeList;
    m_freeList = job->next;
    return job;
}

void JobSystem::freeJob(Job* job)
{
    std::lock_guard<std::mutex> lock(m_poolMutex);
    job->next = m_freeList;
    m_freeList = job;
}

void JobSystem::submit(Job* job, uint32_t preferred)
{
    uint32_t idx = m_tlsWorkerIndex;
    if (idx == UINT32_MAX)
    {
        idx = preferred < m_numWorkers ? preferred : 0;
    }

    Worker& w = *m_workers[idx];
    while (!w.m_deque.push(job))
    {
        Job* temp = nullptr;
        if (w.m_deque.pop(temp))
        {
            executeJob(temp);
        }
        else 
        {
            std::this_thread::yield();
        }
    }

    m_cv.notify_all();
}

bool JobSystem::tryPopJob(uint32_t id, Job*& out)
{
    return m_workers[id]->m_deque.pop(out);
}

bool JobSystem::tryStealJob(uint32_t skipId, Job*& out)
{
    for (uint32_t i = 1; i <= m_numWorkers; ++i) 
    {
        uint32_t victim = (skipId + i) % m_numWorkers;
        if (m_workers[victim]->m_deque.steal(out))
        {
            return true;
        }
    }
    return false;
}

void JobSystem::executeJob(Job* job)
{
    ZoneScopedN("Job");
    char buffer[64];
    snprintf(buffer, sizeof(buffer), "Range: [%u, %u)", job->start, job->end);
    ZoneText(buffer, strlen(buffer));

    for (uint32_t i = job->start; i < job->end; ++i)
    {
        job->fn(job->data, i);
    }

    if (job->counter) 
    {
        if (job->counter->fetch_sub(1) == 1) 
        {
            m_cv.notify_all();
        }
    }

    freeJob(job);
}
