
#pragma once
#ifndef THREAD_POOL_H
#define THREAD_POOL_H

#include <vector>
#include <queue>
#include <atomic>
#include <future>
#include <condition_variable>
#include <thread>
#include <functional>
#include <stdexcept>

namespace std {
#define THREADPOOL_MAX_NUM 16
// #define THREADPOOL_AUTO_GROW

// 线程池，可以提交变参函数 或 lambda 匿名函数执行，可以获取执行返回值
// 不直接支持类成员函数，支持类静态成员函数或全局函数
class threadpool {
private:
    using Task = function<void()>; // 定义任务类型
    vector<thread> pool_; // 线程池
    queue<Task> tasks_; // 任务队列
    mutex lock_; // 同步锁
    condition_variable task_cv_; // 条件阻塞
    atomic<bool> run_{ true }; // 线程池是否执行，原子变量
    atomic<int> idlThrNum_{ 0 }; // 空闲线程数量

public:
    inline threadpool(unsigned short size = 4) { addThread(size); }
    inline ~threadpool() {
        run_ = false;
        task_cv_.notify_all(); // 唤醒所有线程执行
        for (auto& thread : pool_) {
            // thread.detach(); // 让线程“自生自灭”
            if (thread.joinable())
                thread.join(); // 等待任务结束，前提是线程一定会执行完
        }
    }

public:
    // 提交一个任务
    // 调用.get() 获取返回值会等待任务执行完，获取返回值
    // 有两种方法可以实现调用类成员函数，即把类的this 指针给它bind 起来
    // 1. 使用 bind: .commit(std::bind(&Dog::sayHello, &dog));
    // 2. 使用 mem_fn: .commit(std::mem_fn(&Dog::sayHello), this);
    template<class F, class... Args>
    auto commit(F&& f, Args&&... args) -> future<decltype(f(args...))> {
        if (!run_)
            throw runtime_error("commit on ThreadPool is stopped.");

        using RetType = decltype(f(args...)); // typename std::result_of<F(Args...)>::type，函数f的返回类型
        auto task = make_shared<packaged_task<RetType()>>(
            bind(forward<F>(f), forward<Args>(args)...)
        ); // 打包函数入口及参数（绑定
        future<RetType> future = task->get_future();
        {
            lock_guard<mutex> lock{ lock_ }; // 对当前块的语句加锁 lock_guard 是 mutex 的 stack 封装类，构造的时候 lock()，析构的时候 unlock()
            tasks_.emplace([task]() { // push(Task{...}) 放到队列后面
                (*task)();
            });
        }
#ifndef THREADPOOL_AUTO_GROW
        if (idlThrNum_ < 1 && pool_.size() < THREADPOOL_MAX_NUM)
            addThread(1);
#endif // !THREADPOOL_AUTO_GROW
        task_cv_.notify_one(); // 唤醒一个线程执行

        return future;
    }

    // 空闲线程数量
    int idlCount() { return idlThrNum_; }
    // 线程数量
    int thrCount() { return pool_.size(); }

#ifndef THREADPOOL_AUTO_GROW
private:
#endif // !THREADPOOL_AUTO_GROW
    // 添加指定数量的线程
    void addThread(unsigned short size) {
        for (; pool_.size() < THREADPOOL_MAX_NUM && size > 0; --size) {
            pool_.emplace_back([this] {
                while (run_) {
                    Task task;
                    {
                        // unique_lock 相比 lock_guard 的好处是：可以随时 unlock() 和 lock()
                        unique_lock<mutex> lock{ lock_ };
                        task_cv_.wait(lock, [this] {
                            return !run_ || !tasks_.empty();
                        }); // wait 直到有 task
                        if (!run_ && tasks_.empty())
                            return;
                        task = move(tasks_.front()); // 按先进先出从队列中取一个 task
                        tasks_.pop();
                    }
                    --idlThrNum_;
                    task(); // 执行任务
                    ++idlThrNum_;
                }
            });
            ++idlThrNum_;
        }
    }
};

}

#endif
