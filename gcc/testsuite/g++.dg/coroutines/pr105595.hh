// { dg-additional-options "-std=gnu++20" }
// https://gcc.gnu.org/PR105595
#include <coroutine>

class async {
public:
    class promise_type {
    public:
        async get_return_object() { return {}; }
        std::suspend_never initial_suspend() { return {}; }
        std::suspend_never final_suspend() noexcept { return {}; }
        void unhandled_exception() {}
    };
};

namespace {
    class anon_ns_t {};
}

async foo()
{
    anon_ns_t anon_ns;
    class inner_class_t {} inner_class;
    auto a_lambda = [](){};
    co_await std::suspend_never{};
}
