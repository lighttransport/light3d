#ifdef LIGHT3D_CPP20_ENABLED

#include <coroutine>
#include <iostream>
#include <memory>

namespace light3d {

// Simple coroutine example for testing C++20 support
struct Task {
    struct promise_type {
        Task get_return_object() { 
            return Task{std::coroutine_handle<promise_type>::from_promise(*this)}; 
        }
        std::suspend_never initial_suspend() { return {}; }
        std::suspend_never final_suspend() noexcept { return {}; }
        void return_void() {}
        void unhandled_exception() {}
    };
    
    std::coroutine_handle<promise_type> handle;
    
    explicit Task(std::coroutine_handle<promise_type> h) : handle(h) {}
    ~Task() noexcept { if (handle) handle.destroy(); }
    
    Task(const Task&) = delete;
    Task& operator=(const Task&) = delete;
    Task(Task&& other) noexcept : handle(other.handle) { other.handle = nullptr; }
    Task& operator=(Task&& other) noexcept {
        if (this != &other) {
            if (handle) handle.destroy();
            handle = other.handle;
            other.handle = nullptr;
        }
        return *this;
    }
};

// Example async operation
inline Task exampleCoroutine() {
    std::cout << "Coroutine: C++20 support is working!" << std::endl;
    co_return;
}

} // namespace light3d

#endif // LIGHT3D_CPP20_ENABLED
