#include <atomic>
#include <chrono>
#include <condition_variable>
#include <iostream>
#include <queue>
#include <thread>
#include<future>
#include <vector>
#include <unordered_map>


#define USE_NOTIFY 1


enum CmdType {
    INVALID = -1,
    INTERRUPT = 1, // 00001
    MOTION = 2, // 00010
    TOOLING = 4, // 00100
    REQUEST = 8, // 01000
};

std::ostream &operator<<(std::ostream &os, CmdType const &type) {
    switch (type) {
        case CmdType::INTERRUPT:
            os << "INTERRUPT";
            break;
        case CmdType::MOTION:
            os << "MOTION";
            break;
        case CmdType::TOOLING:
            os << "TOOLING";
            break;
        case CmdType::REQUEST:
            os << "REQUEST";
            break;
        default:
            os << "INVALID";
            break;
    }
    return os;
}

class Cmd {
   public:
    Cmd(int i) : i_(i) {
        if (i_ == 0 || i_ == 1) {
            type_ = CmdType::INTERRUPT;
        } else if (i_ == 2 || i_ == 3) {
            type_ = CmdType::MOTION;
        } else if (i_ == 4 || i_ == 5) {
            type_ = CmdType::TOOLING;
        } else if (i_ > 5) {
            type_ = CmdType::REQUEST;
        } else {
            type_ = CmdType::INVALID;
        }
    }

    CmdType getType() const {    
        return type_;
    }

   private:
    int i_;
    CmdType type_;
};

class CmdQueue {
   public:
    CmdQueue() = default;

    void addToQ(Cmd const& i) {
        std::lock_guard<std::mutex> lock(mutex_);

        std::cout << "ADD: " << i.getType() << std::endl;
        cmd_map_[i.getType()].push(i);


        cv_.notify_all();
    }

    Cmd popFromQ(CmdType const type) {
        std::lock_guard<std::mutex> lock(mutex_);

        auto& q = cmd_map_[type];
        auto e = q.front();
        q.pop();
        return e;
    }

    int getStatus() {
        std::lock_guard<std::mutex> lock(mutex_);
        int status = 0;
        for (auto& [type, q] : cmd_map_) {
            if (q.size() > 0) {
                status |= type;
            }
        }
        return status;
    }

#if USE_NOTIFY
    std::condition_variable& getConditionalVariable() { return cv_; }
    std::mutex& getConditionalVariableMutex() { return cv_m_; }
#endif

   private:
    std::mutex mutex_;
    std::unordered_map<CmdType, std::queue<Cmd>> cmd_map_{}; 
    //std::queue<Cmd> q_{};

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
        std::future<void> async_result;
        while (true) {
#if USE_NOTIFY
            std::unique_lock<std::mutex> lk(
                cmd_queue_->getConditionalVariableMutex());
            cmd_queue_->getConditionalVariable().wait(lk, [&] {
                int status = cmd_queue_->getStatus();
                std::cout << "Notifying... " << status << " " << toStr(status) << std::endl;
                std::cout << "Ready to execute... " << toStr(ready_to_execute_) << std::endl;
                return status & ready_to_execute_;
            });
#endif
            int to_execute = cmd_queue_->getStatus() & ready_to_execute_;
            std::cout << "To execute... " << toStr(to_execute) << std::endl;

            if (to_execute > 0) {
                for (int i = 0; i < 4; ++i) {
                    if (to_execute & (1 << i)) {
                        std::cout << "POP: " << cmd_queue_->popFromQ(static_cast<CmdType>(1 << i)).getType() << std::endl;
                    }
                }
            }
            //async_result = std::async(std::launch::async, &ProcessElement::process, this);
        }
    }

   private:
#if 0
    void process() {
        if (cmd_queue_->getSize() > 0) {
            std::cout << "POP: " << cmd_queue_->popFromQ() << std::endl;
        } else {
            std::cout << "Queue empty " << std::endl;
        }
        std::cout << "Processing..." << std::endl;
        std::this_thread::sleep_for(std::chrono::seconds(1));
        std::cout << "Done" << std::endl;
    }
#endif
    
    std::string toStr(int bitmask) {
        
        std::string str;
       
        std::cout << "toStr Bitmask: " << bitmask << std::endl;
        if (bitmask & (1 << CmdType::INTERRUPT)) {
                str += "INTERRUPT |";
            }

        if (bitmask & (1 << CmdType::MOTION)) {
                str += "MOTION |";
            }

        if (bitmask & (1 << CmdType::TOOLING)) {
                str += "TOOLING |";
            }

        if (bitmask & (1 << CmdType::REQUEST)) {
                str += "REQUEST |";
            }
        return str;
    }

   private:
    std::shared_ptr<CmdQueue> cmd_queue_;

    int ready_to_execute_ = 0 | (1 << CmdType::INTERRUPT) | (1 << CmdType::MOTION) | (1 << CmdType::TOOLING) | (1 << CmdType::REQUEST);

    //std::vector<std::future<>>
};

void add_elements(std::shared_ptr<CmdQueue> q) {
    using namespace std::chrono_literals;

    //int i{0};
    std::srand(static_cast<unsigned int>(std::time(nullptr)));
    while (true) {
        int i = std::rand() % 10;
        std::cout << "ADD: " << i << std::endl;
        q->addToQ(i);
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
