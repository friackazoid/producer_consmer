#include <chrono>
#include <condition_variable>
#include <iostream>
#include <queue>
#include <thread>

#define USE_NOTIFY 1

class CmdQueue {
   public:
    CmdQueue() = default;

    void addToQ(int i) {
        std::lock_guard<std::mutex> lock(mutex_);
        q_.push(i);
#if USE_NOTIFY
        cv_.notify_all();
#endif
    }

    int popFromQ() {
        std::lock_guard<std::mutex> lock(mutex_);
        auto e = q_.front();
        q_.pop();
        return e;
    }

    unsigned getSize() {
        std::lock_guard<std::mutex> lock(mutex_);
        return q_.size();
    }

#if USE_NOTIFY
    std::condition_variable& getConditionalVariable() { return cv_; }
    std::mutex& getConditionalVariableMutex() { return cv_m_; }
#endif

   private:
    std::mutex mutex_;
    std::queue<int> q_{};

#if USE_NOTIFY
    std::mutex cv_m_;
    std::condition_variable cv_;
#endif
};

class ProcessElement {
   public:
    ProcessElement(std::shared_ptr<CmdQueue> cmd_queue)
        : cmd_queue_(cmd_queue) {}

    void run() {
        while (true) {
#if USE_NOTIFY
            std::unique_lock<std::mutex> lk(
                cmd_queue_->getConditionalVariableMutex());
            cmd_queue_->getConditionalVariable().wait(lk, [&] {
                std::cout << "Notifying... " << cmd_queue_->getSize()
                          << std::endl;
                return cmd_queue_->getSize() > 0;
            });
#endif

            if (cmd_queue_->getSize() > 0) {
                std::cout << "POP: " << cmd_queue_->popFromQ() << std::endl;
            } else {
                std::cout << "Queue empty " << std::endl;
            }
        }
    }

   private:
    std::shared_ptr<CmdQueue> cmd_queue_;
};

void add_elements(std::shared_ptr<CmdQueue> q) {
    using namespace std::chrono_literals;

    int i{0};
    while (true) {
        std::cout << "ADD: " << i << std::endl;
        q->addToQ(i++);
        std::this_thread::sleep_for(5s);
    }
}

int main() {
    auto cmd_queue = std::make_shared<CmdQueue>();

    ProcessElement process_element(cmd_queue);

    std::thread th(add_elements, cmd_queue);
    process_element.run();

    th.join();
}
