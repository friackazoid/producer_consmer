// g++ test_packaged_task.cpp -o test_packaged_task -lpthread -std=c++17

#include <future>
#include <iostream>
#include <memory>
#include <thread>
#include <queue>
#include <mutex>
#include <vector>

class Cmd {
   public:
   explicit Cmd(int i) : i_(i), str_("msg_"+std::to_string(i)) {}

   friend std::ostream& operator<<(std::ostream& os, const Cmd& cmd) {
        os << cmd.str_;
        return os;
    }

   private:
    int i_;
    std::string str_;
};

class ThreadSafeQueue {
    public:
        ThreadSafeQueue() = default;
        ~ThreadSafeQueue() {
            std::lock_guard<std::mutex> lock(mutex_);
            while (!queue_.empty()) {
                std::cout << "deleting " << queue_.front() << std::endl;
                queue_.pop();
            }
        };

        void add(Cmd cmd) {
            std::lock_guard<std::mutex> lock(mutex_);
            std::cout << "adding " << cmd << std::endl;
            queue_.push(cmd);
        }

        Cmd get() {
            std::lock_guard<std::mutex> lock(mutex_);
            Cmd cmd = queue_.front();
            queue_.pop();
            return cmd;
        }

    private:
        std::mutex mutex_;
        std::queue<Cmd> queue_;
};

class Executor {

    public:
        explicit Executor(std::shared_ptr<ThreadSafeQueue> queue) : queue_(queue) {}
        ~Executor() = default;

        void run() {
            while (true) {
                Cmd cmd = queue_->get();
                std::packaged_task<bool()> task([this, &cmd]() {
                    return process_element(cmd);
                });

                futures_.push_back(std::move(task.get_future()));
            }
        }

        void wait() {
            using namespace std::chrono_literals;
            for (auto& future : futures_) {
                auto status = future.wait_for( 10ms );
                if (status == std::future_status::ready) {
                    std::cout << "future is ready; result: " << future.get() << std::endl;
                } else if (status == std::future_status::timeout) {
                    std::cout << "future is not ready" << std::endl;
                } else if (status == std::future_status::deferred) {
                    std::cout << "future is deferred" << std::endl;
                }
            }
        }

    private:
        bool process_element(Cmd const& cmd)  {
            std::cout << "executing " << cmd << std::endl;
            // sleep for random time
            std::this_thread::sleep_for(std::chrono::seconds(std::rand() % 10));
            return true;
        }

    private:
        std::shared_ptr<ThreadSafeQueue> queue_;

        std::vector<std::future<bool>> futures_;
    
};

void  add_elements (std::shared_ptr<ThreadSafeQueue> queue) {

    std::srand(static_cast<unsigned int>(std::time(nullptr)));

    while (true) {
        queue->add(Cmd(std::rand() % 10));
        // sleep for random time
        std::this_thread::sleep_for(std::chrono::seconds(std::rand() % 10));
    }
}

int main (int /*argc*/, char**  /*argv*/) {

    auto queue = std::make_shared<ThreadSafeQueue> ();
   Executor executor(queue);

    // create a thread to add elements to the queue
    std::thread t1(add_elements, std::ref(queue));

    executor.run();

    std::thread t2 ( [&executor] () {
            while (true) {
                executor.wait();
        }
    });

    t1.join();
    t2.join();
    return 0;
}
