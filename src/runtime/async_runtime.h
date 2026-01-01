// async_runtime.h -- Ardent 2.4 "Living Chronicles"
// Async execution, task scheduling, and continuation support
#ifndef ARDENT_ASYNC_RUNTIME_H
#define ARDENT_ASYNC_RUNTIME_H

#include <functional>
#include <memory>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <atomic>
#include <chrono>
#include <optional>
#include <variant>
#include <vector>
#include <unordered_map>

namespace ardent {
namespace async {

// ─── Forward Declarations ──────────────────────────────────────────────────

class Task;
class Promise;
class Scheduler;
class TaskQueue;

// ─── Task State ────────────────────────────────────────────────────────────

enum class TaskState {
    Pending,      // Created but not started
    Running,      // Currently executing
    Suspended,    // Awaiting something
    Completed,    // Finished successfully
    Failed,       // Finished with error
    Cancelled     // Explicitly cancelled
};

inline const char* taskStateToString(TaskState s) {
    switch (s) {
        case TaskState::Pending: return "Pending";
        case TaskState::Running: return "Running";
        case TaskState::Suspended: return "Suspended";
        case TaskState::Completed: return "Completed";
        case TaskState::Failed: return "Failed";
        case TaskState::Cancelled: return "Cancelled";
        default: return "Unknown";
    }
}

// ─── Async Value ───────────────────────────────────────────────────────────

// Forward declare ArdentValue or use a variant
struct AsyncValue {
    enum class Type { Void, Number, String, Boolean, Error };
    Type type = Type::Void;
    double numVal = 0.0;
    std::string strVal;
    bool boolVal = false;
    
    AsyncValue() = default;
    explicit AsyncValue(double n) : type(Type::Number), numVal(n) {}
    explicit AsyncValue(const std::string& s) : type(Type::String), strVal(s) {}
    explicit AsyncValue(bool b) : type(Type::Boolean), boolVal(b) {}
    
    static AsyncValue error(const std::string& msg) {
        AsyncValue v;
        v.type = Type::Error;
        v.strVal = msg;
        return v;
    }
    
    bool isError() const { return type == Type::Error; }
    bool isVoid() const { return type == Type::Void; }
};

// ─── Continuation ──────────────────────────────────────────────────────────

// A continuation represents the "rest of the computation" after an await point
using Continuation = std::function<void(AsyncValue)>;

// ─── Promise ───────────────────────────────────────────────────────────────

class Promise {
public:
    using Id = uint64_t;
    
    Promise() : id_(nextId_++) {}
    
    Id id() const { return id_; }
    
    bool isResolved() const { return resolved_; }
    bool isRejected() const { return rejected_; }
    bool isPending() const { return !resolved_ && !rejected_; }
    
    void resolve(AsyncValue value) {
        if (resolved_ || rejected_) return;
        value_ = std::move(value);
        resolved_ = true;
        notifyWaiters();
    }
    
    void reject(const std::string& error) {
        if (resolved_ || rejected_) return;
        value_ = AsyncValue::error(error);
        rejected_ = true;
        notifyWaiters();
    }
    
    const AsyncValue& value() const { return value_; }
    
    void onResolve(Continuation cont) {
        if (resolved_ || rejected_) {
            cont(value_);
        } else {
            waiters_.push_back(std::move(cont));
        }
    }
    
private:
    void notifyWaiters() {
        for (auto& w : waiters_) {
            w(value_);
        }
        waiters_.clear();
    }
    
    static inline std::atomic<Id> nextId_{1};
    Id id_;
    bool resolved_ = false;
    bool rejected_ = false;
    AsyncValue value_;
    std::vector<Continuation> waiters_;
};

// ─── Task ──────────────────────────────────────────────────────────────────

class Task {
public:
    using Id = uint64_t;
    using TaskFn = std::function<void()>;
    
    Task(TaskFn fn, const std::string& name = "anonymous")
        : id_(nextId_++), name_(name), fn_(std::move(fn)), state_(TaskState::Pending) {}
    
    Id id() const { return id_; }
    const std::string& name() const { return name_; }
    TaskState state() const { return state_; }
    
    void setState(TaskState s) { state_ = s; }
    
    void run() {
        if (state_ == TaskState::Cancelled) return;
        state_ = TaskState::Running;
        try {
            fn_();
            if (state_ == TaskState::Running) {
                state_ = TaskState::Completed;
            }
        } catch (const std::exception& e) {
            error_ = e.what();
            state_ = TaskState::Failed;
        }
    }
    
    void suspend() { state_ = TaskState::Suspended; }
    void resume() { if (state_ == TaskState::Suspended) state_ = TaskState::Pending; }
    void cancel() { state_ = TaskState::Cancelled; }
    
    const std::string& error() const { return error_; }
    
    // For continuation-based suspension
    void setContinuation(Continuation cont) { continuation_ = std::move(cont); }
    bool hasContinuation() const { return continuation_ != nullptr; }
    void invokeContinuation(AsyncValue val) {
        if (continuation_) {
            auto cont = std::move(continuation_);
            continuation_ = nullptr;
            cont(std::move(val));
        }
    }
    
private:
    static inline std::atomic<Id> nextId_{1};
    Id id_;
    std::string name_;
    TaskFn fn_;
    TaskState state_;
    std::string error_;
    Continuation continuation_;
};

// ─── Task Queue ────────────────────────────────────────────────────────────

class TaskQueue {
public:
    void push(std::shared_ptr<Task> task) {
        std::lock_guard<std::mutex> lock(mutex_);
        queue_.push(std::move(task));
        cv_.notify_one();
    }
    
    std::shared_ptr<Task> pop() {
        std::unique_lock<std::mutex> lock(mutex_);
        cv_.wait(lock, [this] { return !queue_.empty() || stopped_; });
        if (stopped_ && queue_.empty()) return nullptr;
        auto task = std::move(queue_.front());
        queue_.pop();
        return task;
    }
    
    std::shared_ptr<Task> tryPop() {
        std::lock_guard<std::mutex> lock(mutex_);
        if (queue_.empty()) return nullptr;
        auto task = std::move(queue_.front());
        queue_.pop();
        return task;
    }
    
    bool empty() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return queue_.empty();
    }
    
    size_t size() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return queue_.size();
    }
    
    void stop() {
        std::lock_guard<std::mutex> lock(mutex_);
        stopped_ = true;
        cv_.notify_all();
    }
    
    void clear() {
        std::lock_guard<std::mutex> lock(mutex_);
        while (!queue_.empty()) queue_.pop();
    }
    
private:
    mutable std::mutex mutex_;
    std::condition_variable cv_;
    std::queue<std::shared_ptr<Task>> queue_;
    bool stopped_ = false;
};

// ─── Timer Entry ───────────────────────────────────────────────────────────

struct TimerEntry {
    std::chrono::steady_clock::time_point deadline;
    std::shared_ptr<Promise> promise;
    
    bool operator>(const TimerEntry& other) const {
        return deadline > other.deadline;
    }
};

// ─── Scheduler ─────────────────────────────────────────────────────────────

class Scheduler {
public:
    Scheduler() : running_(false) {}
    
    ~Scheduler() {
        stop();
    }
    
    // Spawn a new task
    Task::Id spawn(Task::TaskFn fn, const std::string& name = "task") {
        auto task = std::make_shared<Task>(std::move(fn), name);
        Task::Id id = task->id();
        {
            std::lock_guard<std::mutex> lock(tasksMutex_);
            tasks_[id] = task;
        }
        readyQueue_.push(task);
        return id;
    }
    
    // Schedule a timer (returns promise that resolves after ms milliseconds)
    std::shared_ptr<Promise> sleep(uint64_t ms) {
        auto promise = std::make_shared<Promise>();
        auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(ms);
        {
            std::lock_guard<std::mutex> lock(timersMutex_);
            timers_.push({deadline, promise});
        }
        return promise;
    }
    
    // Get current monotonic time in milliseconds
    uint64_t now() const {
        auto epoch = std::chrono::steady_clock::now().time_since_epoch();
        return std::chrono::duration_cast<std::chrono::milliseconds>(epoch).count();
    }
    
    // Get wall clock time as ISO string
    std::string wallClock() const {
        auto now = std::chrono::system_clock::now();
        auto time = std::chrono::system_clock::to_time_t(now);
        char buf[64];
        std::strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%S", std::localtime(&time));
        return buf;
    }
    
    // Run the scheduler (blocking)
    void run() {
        running_ = true;
        while (running_) {
            // Process ready timers
            processTimers();
            
            // Process ready tasks
            auto task = readyQueue_.tryPop();
            if (task) {
                task->run();
                
                // Remove completed/failed/cancelled tasks
                if (task->state() == TaskState::Completed ||
                    task->state() == TaskState::Failed ||
                    task->state() == TaskState::Cancelled) {
                    std::lock_guard<std::mutex> lock(tasksMutex_);
                    tasks_.erase(task->id());
                }
            } else if (hasPendingWork()) {
                // Sleep briefly if no ready tasks but work pending
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            } else {
                // No work at all, exit
                break;
            }
        }
    }
    
    // Run one iteration (for integration with REPL)
    bool tick() {
        processTimers();
        
        auto task = readyQueue_.tryPop();
        if (task) {
            task->run();
            if (task->state() == TaskState::Completed ||
                task->state() == TaskState::Failed ||
                task->state() == TaskState::Cancelled) {
                std::lock_guard<std::mutex> lock(tasksMutex_);
                tasks_.erase(task->id());
            }
            return true;
        }
        return false;
    }
    
    void stop() {
        running_ = false;
        readyQueue_.stop();
    }
    
    bool isRunning() const { return running_; }
    
    // Task management
    std::optional<TaskState> taskState(Task::Id id) const {
        std::lock_guard<std::mutex> lock(tasksMutex_);
        auto it = tasks_.find(id);
        if (it == tasks_.end()) return std::nullopt;
        return it->second->state();
    }
    
    bool cancelTask(Task::Id id) {
        std::lock_guard<std::mutex> lock(tasksMutex_);
        auto it = tasks_.find(id);
        if (it == tasks_.end()) return false;
        it->second->cancel();
        return true;
    }
    
    std::vector<std::pair<Task::Id, std::string>> listTasks() const {
        std::lock_guard<std::mutex> lock(tasksMutex_);
        std::vector<std::pair<Task::Id, std::string>> result;
        for (const auto& [id, task] : tasks_) {
            result.push_back({id, task->name() + " [" + taskStateToString(task->state()) + "]"});
        }
        return result;
    }
    
    size_t pendingTaskCount() const {
        std::lock_guard<std::mutex> lock(tasksMutex_);
        return tasks_.size();
    }
    
private:
    void processTimers() {
        auto now = std::chrono::steady_clock::now();
        std::lock_guard<std::mutex> lock(timersMutex_);
        
        while (!timers_.empty() && timers_.top().deadline <= now) {
            auto entry = timers_.top();
            timers_.pop();
            entry.promise->resolve(AsyncValue());
        }
    }
    
    bool hasPendingWork() const {
        if (!readyQueue_.empty()) return true;
        std::lock_guard<std::mutex> lock(timersMutex_);
        return !timers_.empty();
    }
    
    std::atomic<bool> running_;
    TaskQueue readyQueue_;
    
    mutable std::mutex tasksMutex_;
    std::unordered_map<Task::Id, std::shared_ptr<Task>> tasks_;
    
    mutable std::mutex timersMutex_;
    std::priority_queue<TimerEntry, std::vector<TimerEntry>, std::greater<TimerEntry>> timers_;
};

// ─── Global Scheduler Access ───────────────────────────────────────────────

inline Scheduler& globalScheduler() {
    static Scheduler scheduler;
    return scheduler;
}

} // namespace async
} // namespace ardent

#endif // ARDENT_ASYNC_RUNTIME_H
