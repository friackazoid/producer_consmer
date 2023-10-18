// g++ test_packaged_task_5.cpp -o test_packaged_task_5 -lpthread -std=c++17

#include <chrono>
#include <iostream>
#include <future>
#include <thread>
#include <functional>
#include <queue>
#include <map>

class Foo {

    public:
        void worker() {
            while (true) {

                if (!queue_.empty()) {
 
                    std::packaged_task<bool(int)> task(std::bind(&Foo::do_stuff, this, std::placeholders::_1));
                    
                    {
                        std::lock_guard<std::mutex> lock(mutex_);
                        int i = queue_.front();
                        std::cout << "Create future for " << i << std::endl;
                        futures_[i] = task.get_future();
                        queue_.pop();
                    }

                    // run task with random argument
                    std::thread(std::move(task), rand() % 10).detach(); 
                }
                // sleep random time
                std::this_thread::sleep_for(std::chrono::milliseconds(rand() % 1000));
            }
        }

        void wait_for () {

            std::cout << "Start wait_for" << std::endl;
            while (true) {
                for (auto& [k ,f] : futures_) {
                    if (!f.valid()) {
                        std::cout << k << " is not valid" << std::endl;
                        continue;
                    }
                    std::future_status status;
                    switch (status = f.wait_for(std::chrono::milliseconds(100)); status)
                    {
                        case std::future_status::deferred:
                            std::cout << "deferred" << std::endl;
                            break;
                        case std::future_status::timeout:
                            std::cout << k << " is not yet ready; timeout" << std::endl;
                            break;
                        case std::future_status::ready:
                            std::cout << k<< " is ready; result " << f.get() << std::endl;

                            std::cout << "return "<< k << " to queue" << std::endl;

                            {
                                std::lock_guard<std::mutex> lock(mutex_);
                                queue_.push(k);
                            }
                            break;
                        }
                }
            }
        }

        bool do_stuff (int a) {
            std::cout << "Start do_stuff: " << a << std::endl;
            // sleep random time
            std::this_thread::sleep_for(std::chrono::milliseconds(rand() % 1000));
            std::cout << "Finish do_stuff: " << a << std::endl;

            return true;
        }

    private:
        std::mutex mutex_;
        std::queue<int> queue_{{0, 1,2,3,4}};
        std::map<int, std::future<bool>> futures_;

};

int main () {

    Foo foo;

    std::thread t1 (&Foo::worker, &foo);
    std::thread t2 (&Foo::wait_for, &foo);

    t1.join();
    t2.join();

    return 0;
}
