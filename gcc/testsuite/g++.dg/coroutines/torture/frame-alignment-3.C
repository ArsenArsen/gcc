// { dg-do run }
// { dg-additional-options "-Wp,-fdump-lang-coro" }
#include <cstdint>
#include <cassert>
#include <exception>
#include <coroutine>
#include <unordered_map>

/* This test checks that our synthesized coroutine allocation correctly handles
   the padding at the start of frames.  */

static std::unordered_map<void*, std::ptrdiff_t> off_map;
static std::unordered_map<void*, std::size_t> sz_map;

#pragma GCC diagnostic ignored "-Wpointer-arith"

static constexpr auto overalignment = 2 * __STDCPP_DEFAULT_NEW_ALIGNMENT__;

struct task
{
  struct alignas(overalignment) promise_type
  {
    promise_type ()
    {
      if (((std::uintptr_t)this) % overalignment)
	std::terminate ();
    }

    task get_return_object () noexcept {
      return { std::coroutine_handle<promise_type>::from_promise (*this) };
    }
    void unhandled_exception () noexcept {}
    std::suspend_never initial_suspend () { return {}; }
    std::suspend_never final_suspend () noexcept { return {}; }
    void return_void () {}

    void
    operator delete (void* ptr, std::size_t sz)
    {
      auto off = off_map.at (ptr);
      off_map.erase (ptr);
      ::operator delete (ptr - off, sz + __STDCPP_DEFAULT_NEW_ALIGNMENT__);
    }

    void*
    operator new (std::size_t sz)
    {
      auto x = ::operator new (sz + __STDCPP_DEFAULT_NEW_ALIGNMENT__);
      std::ptrdiff_t off = 0;

      if (((std::uintptr_t)x) % overalignment == 0)
	x += (off = __STDCPP_DEFAULT_NEW_ALIGNMENT__);

      off_map.emplace (x, off);
      return x;
    }
  };

  std::coroutine_handle<promise_type> handle;
};

static int* aa_loc;

task
foo ()
{
  int aa;
  aa_loc = &aa;
  co_await std::suspend_always{};
  assert (&aa == aa_loc);
  co_return;
}

int
main ()
{
  auto ro = foo ();
  ro.handle ();

  auto ro2 = foo ();
  ro2.handle.destroy ();

  assert (off_map.empty ());
}

// Ensure that we're actually testing something.
// { dg-final { scan-lang-dump "\n_Z.*\\.Frame\\s*\\{\[^\}\]*_Coro_padding\[^\}\]*\\}" coro } }
