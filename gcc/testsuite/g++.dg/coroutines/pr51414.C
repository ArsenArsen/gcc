#include <cstdio>
#include <utility>
#include <exception>
#include <optional>

#ifdef _LIBCPP_VERSION
#include <experimental/coroutine>
namespace std {
using std::experimental::coroutine_handle;
using std::experimental::suspend_never;
using std::experimental::suspend_always;
using std::experimental::noop_coroutine;
}
#else
#include <coroutine>
#endif

namespace Co {

template<typename Promise=void>
class UniqueHandle {
	std::coroutine_handle<Promise> value;

public:
	UniqueHandle() = default;

	explicit constexpr UniqueHandle(std::coroutine_handle<Promise> h) noexcept
		:value(h) {}

	UniqueHandle(UniqueHandle<Promise> &&src) noexcept
		:value(std::exchange(src.value, nullptr))
	{
	}

	~UniqueHandle() noexcept {
		if (value)
			value.destroy();
	}

	auto &operator=(UniqueHandle<Promise> &&src) noexcept {
		using std::swap;
		swap(value, src.value);
		return *this;
	}

	operator bool() const noexcept {
		return (bool)value;
	}

	const auto &get() const noexcept {
		return value;
	}

	const auto *operator->() const noexcept {
		return &value;
	}
};

template<typename T, typename Task>
class promise {
	std::coroutine_handle<> continuation;

	std::optional<T> value;
	std::exception_ptr error;

public:
	auto initial_suspend() noexcept {
		return std::suspend_always{};
	}

	struct final_awaitable {
		bool await_ready() const noexcept {
			return false;
		}

		template<typename PROMISE>
		std::coroutine_handle<> await_suspend(std::coroutine_handle<PROMISE> coro) noexcept {
			const auto &promise = coro.promise();
			return promise.continuation;
		}

		void await_resume() noexcept {
		}
	};

	auto final_suspend() noexcept {
		return final_awaitable{};
	}

	auto get_return_object() noexcept {
		return Task(std::coroutine_handle<promise>::from_promise(*this));
	}

	void unhandled_exception() noexcept {
		error = std::current_exception();
	}

	template<typename U>
	void return_value(U &&_value) noexcept {
		value.emplace(std::forward<U>(_value));
	}

private:
	void SetContinuation(std::coroutine_handle<> _continuation) noexcept {
		continuation = _continuation;
	}

	decltype(auto) GetReturnValue() {
		if (error)
			std::rethrow_exception(std::move(error));

		return std::move(*value);
	}

public:
	struct Awaitable final {
		const std::coroutine_handle<promise> coroutine;

		bool await_ready() const noexcept {
			return coroutine.done();
		}

		std::coroutine_handle<> await_suspend(std::coroutine_handle<> _continuation) noexcept {
			coroutine.promise().SetContinuation(_continuation);
			return coroutine;
		}

		T await_resume() {
			return coroutine.promise().GetReturnValue();
		}
	};
};

template<typename T>
class Task {
public:
	using promise_type = promise<T, Task<T>>;
	friend promise_type;

private:
	UniqueHandle<promise_type> coroutine;

	explicit Task(std::coroutine_handle<promise_type> _coroutine) noexcept
		:coroutine(_coroutine)
	{
	}

public:
	Task() = default;

	typename promise_type::Awaitable operator co_await() const noexcept {
		return {coroutine.get()};
	}
};

} // namespace Co

struct Foo {};

Co::Task<Foo> FooA() {
	co_return Foo{};
}

Co::Task<Foo> FooB() {
	co_return Foo{};
}

Co::Task<Foo> FooC(bool a)
{
	co_return co_await (a ? FooA() : FooB());
}

int main() {
	auto task = FooC(true);
	auto awaitable = task.operator co_await();
	if (!awaitable.await_ready())
		awaitable.await_suspend(std::noop_coroutine()).resume();
	awaitable.await_resume();
}
