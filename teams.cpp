#include <utility>
#include <deque>
#include <future>
#include <list>
#include <inttypes.h>
#include "teams.hpp"
#include "contest.hpp"
#include "collatz.hpp"

 uint64_t Team::calcCollatzRecursive(InfInt n, std::shared_ptr<SharedResults> shared)
{
    // It's ok even if the value overflow
//    printf("n = %lu, count = %lu \n", n.toUnsignedLong(), count);

    assert(n > 0);
    if (n == 1) {
        printf("ssssssssssssssssssssss\n");
        return 0;
    }
    else {
        printf("zzzzzzzzzzzzzzzzzzzzzzz\n");
        InfInt new_n;
        uint64_t c = shared->getValue(n.toUnsignedLong());
        if (c != 0) return c;
        // else
        if (n % 2 == 1) {
            new_n = n * 3;
            new_n += 1;
        } else {
            new_n = n / 2;
        }
        printf("new_n == %lu\n", new_n.toUnsignedLong());
        uint64_t count = calcCollatzRecursive(new_n, shared);
        shared->setValue(n.toUnsignedLong(), count + 1);
        return count + 1;
//        return 1;
    }
}

void TeamNewThreads::insertCollatz(ContestResult & result, uint64_t i, uint64_t idx, InfInt const & n,
                                            std::queue<uint64_t>& readyToJoin, std::mutex& mutex,
                                            std::condition_variable& cond) {
    result[idx] = calcCollatz(n);
    std::unique_lock<std::mutex> lock(mutex);
    readyToJoin.push(i);
    lock.unlock();
    cond.notify_all();
}

/* implementacja TeamNewThreads::runContestImpl,
 * gdzie podjeta zostala proba ambitniejszego wyboru watkow
 * na ktorych wolamy join() przy zapelnieniu puli.
 */
ContestResult TeamNewThreads::runContestImpl(ContestInput const & contestInput)
{
    ContestResult result;
    result.resize(contestInput.size());
    uint64_t idx = 0;
    std::queue<uint64_t> readyToJoin;
    std::condition_variable cond;
    std::mutex cond_mutex; // mutex for the condition variable cond

    uint32_t maxThreads = this->getSize();
    std::thread threads[maxThreads];
    uint64_t i = 0;
    uint64_t waitingThreads = 0;
    for(InfInt const & singleInput : contestInput)
    {
        if (i < maxThreads) {
            threads[i] = createThread([&result, i, idx, &singleInput, &readyToJoin, &cond_mutex, &cond]
                    { insertCollatz(result, i, idx, singleInput, readyToJoin, cond_mutex, cond); });
        } else {
            std::unique_lock<std::mutex> lock(cond_mutex);
            waitingThreads++;
            cond.wait(lock, [&readyToJoin]{ return !readyToJoin.empty(); });
            uint64_t j = readyToJoin.front();
            threads[j].join();
            waitingThreads--;
            readyToJoin.pop();
            threads[j] = createThread([&result, j, idx, &singleInput, &readyToJoin, &cond_mutex, &cond]
                                      { insertCollatz(result, j, idx, singleInput, readyToJoin, cond_mutex, cond); });
        }
        ++idx;
        ++i;
    }
    uint32_t min;
    if (maxThreads > i) min = i;
    else min = maxThreads;
    for (int k = 0; k < min; k++) {
        threads[k].join();
    }
    return result;
}

// wersja, gdzie join() wolany jest na watkach po kolei od poczatku
//ContestResult TeamNewThreads::runContestImpl(ContestInput const & contestInput)
//{
//    ContestResult result;
//    result.resize(contestInput.size());
//    uint64_t idx = 0;
//
//    uint32_t maxThreads = this->getSize();
//    std::thread threads[maxThreads];
//    uint64_t i = 0;
//    bool threadsFull = false;
//    for (InfInt const & singleInput : contestInput)
//    {
//        if (threadsFull) {
//            threads[i].join();
//        }
//        threads[i] = createThread([&result, i, idx, &singleInput]
//                                  { result[idx] = calcCollatz(singleInput); });
//        ++idx;
//        ++i;
//        if (i == maxThreads) {
//            threadsFull = true;
//            i = 0;
//        }
//    }
//    uint32_t min;
//    if (maxThreads > idx) min = idx;
//    else min = maxThreads;
//    for (int k = 0; k < min; k++) {
//        threads[k].join();
//    }
//    return result;
//}

void TeamConstThreads::insertCollatz(ContestResult & result,
                                     ContestInput const & input,
                                     size_t threadsNum,
                                     size_t myNum) {

    for (size_t i = myNum; i < input.size(); i+= threadsNum) {
        result[i] = calcCollatz(input[i]);
    }
}

ContestResult TeamConstThreads::runContestImpl(ContestInput const & contestInput)
{
    ContestResult r;
    r.resize(contestInput.size());
    size_t threadsNum = this->getSize();
//    printf("getSize = %u", this->getSize());

    std::thread threads[threadsNum];
    for (size_t thIndex = 0; thIndex < threadsNum; thIndex++) {
//        printf("THREAD %zu CREATED\n", thIndex);
        threads[thIndex] = createThread([&threadsNum, &r, &contestInput, thIndex ]
                { insertCollatz(r, contestInput, threadsNum, thIndex); });
    }
    for (size_t thIndex = 0; thIndex < threadsNum; thIndex++) {
        threads[thIndex].join();
    }
    return r;
}

ContestResult TeamPool::runContest(ContestInput const & contestInput)
{
    ContestResult r;
    r.resize(contestInput.size());

    std::vector<std::future<void>> futures;
    int i = 0;
    for(InfInt const & singleInput : contestInput) {
        futures.push_back(this->pool.push([&singleInput, i, &r]{r[i] = calcCollatz(singleInput);}));
        i++;
    }
    for (auto& f : futures) {
        f.get();
    }
    return r;
}

ContestResult TeamNewProcesses::runContest(ContestInput const & contestInput)
{
    ContestResult r;
    //TODO
    return r;
}

ContestResult TeamConstProcesses::runContest(ContestInput const & contestInput)
{
    ContestResult r;
    //TODO
    return r;
}

ContestResult TeamAsync::runContest(ContestInput const & contestInput)
{
    ContestResult r;
    r.resize(contestInput.size());

    std::vector<std::future<void>> futures;
    int i = 0;
    if (!this->getSharedResults()) {
        for(InfInt const & singleInput : contestInput) {
            futures.push_back(std::async(std::launch::async, [&singleInput, i, &r]{ r[i] = calcCollatz(singleInput); }));
            i++;
        }
    } else {
        for(InfInt const & singleInput : contestInput) {
//            futures.push_back(std::async(std::launch::async,[&singleInput, i, &r, this]
//                                { r[i] = calcCollatzRecursive(singleInput, 0, this->getSharedResults()); }));
//            futures.push_back(std::async([&singleInput, i, &r, this]
//            { r[i] = calcCollatzRecursive(singleInput, 0, this->getSharedResults()); }));
            std::shared_ptr<SharedResults> sharedPtr = this->getSharedResults();
            if (sharedPtr == NULL) printf("NULL\n");
//            sharedPtr->print(5);
            r[i] = calcCollatzRecursive(singleInput, sharedPtr);
            i++;
        }
    }
    for (auto& f : futures) {
        f.get();
    }
    return r;
}
