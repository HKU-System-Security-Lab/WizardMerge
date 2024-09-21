#ifndef THREAD_POOL_H
#define THREAD_POOL_H

#include <vector>
#include <queue>
#include <memory>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <future>
#include <functional>
#include <stdexcept>
#include <sys/time.h>
#include <fstream>
#include <string>

const uint64_t Thread_kUsToS = 1000000;

class ThreadPool {
public:
    ThreadPool(size_t);
    template<class F, class... Args>
    auto enqueue(int id, F&& f, Args&&... args) 
        -> std::future<typename std::result_of<F(Args...)>::type>;
    ~ThreadPool();
private:
    // need to keep track of threads so we can join them
    std::vector< std::thread > workers;
    // the task queue
    std::queue< std::pair< int, std::function<void()> > > tasks;
    
    // synchronization
    std::mutex queue_mutex;
    std::condition_variable condition;
    bool stop;
};
 
// the constructor just launches some amount of workers
inline ThreadPool::ThreadPool(size_t threads)
    :   stop(false)
{
    for(size_t i = 0;i<threads;++i)
        workers.emplace_back(
            [this, i]
            {
                for(;;)
                {
                    std::pair< int, std::function<void()> > task;

                    {
                        std::unique_lock<std::mutex> lock(this->queue_mutex);
                        this->condition.wait(lock,
                            [this]{ return this->stop || !this->tasks.empty(); });
                        if(this->stop && this->tasks.empty())
                            return;
                        task = std::move(this->tasks.front());
                        this->tasks.pop();

                        auto *thread_status_path = getenv("CMRA_THREAD_STATUS");
                        if (thread_status_path != nullptr) {
                            std::ofstream myfile;
                            myfile.open(thread_status_path, std::ios::app);
                            myfile << this->tasks.size() << std::endl;
                            myfile.close();
                        }
                    }
                    struct timeval tv;
                    gettimeofday(&tv, NULL);
                    double start = (double) (tv.tv_sec * Thread_kUsToS + tv.tv_usec)/ (double)Thread_kUsToS;
                    task.second();
                    gettimeofday(&tv, NULL);
                    double end = (double) (tv.tv_sec * Thread_kUsToS + tv.tv_usec) / (double)Thread_kUsToS;
                    double elapsed = end - start;
                    auto *enable_thread_log = getenv("CMRA_ENABLE_THREAD_LOG");
                    if (enable_thread_log != nullptr) {
                        std::ofstream myfile;
                        myfile.open("./execution_log_" + std::to_string(i), std::ios::app);
                        myfile.setf(std::ios::fixed, std::ios::floatfield);
                        myfile << task.first << ": start -> " << start << " end -> " << end << " solving_time -> "<< elapsed << std::endl;
                        myfile.close();
                    }
                    
                }
            }
        );
}

// add new work item to the pool
template<class F, class... Args>
auto ThreadPool::enqueue(int id, F&& f, Args&&... args) 
    -> std::future<typename std::result_of<F(Args...)>::type>
{
    using return_type = typename std::result_of<F(Args...)>::type;

    auto task = std::make_shared< std::packaged_task<return_type()> >(
            std::bind(std::forward<F>(f), std::forward<Args>(args)...)
        );
        
    std::future<return_type> res = task->get_future();
    {
        std::unique_lock<std::mutex> lock(queue_mutex);

        // don't allow enqueueing after stopping the pool
        if(stop)
            throw std::runtime_error("enqueue on stopped ThreadPool");
        // printf("[enqueue] id - > %d is ready to enqueue\n", id);
        tasks.emplace(std::make_pair(id, [task](){ (*task)(); }));
    }
    condition.notify_one();
    return res;
}

// the destructor joins all threads
inline ThreadPool::~ThreadPool()
{
    {
        std::unique_lock<std::mutex> lock(queue_mutex);
        stop = true;
    }
    condition.notify_all();

    for (int i = 0; i < workers.size(); i++) {
        workers[i].join();
        // printf("[join] id -> %d is ready to get out\n", i);
    }

    // for(std::thread &worker: workers)
    //     worker.join();
}

#endif
