// { dg-do run }
// { dg-additional-options "-Wp,-fdump-lang-coro" }
#include <cstdint>
#include <cassert>
#include <exception>
#include <coroutine>
#include <unordered_map>

/* This test checks that our synthesized coroutine allocation correctly aligns
   the promise type inside the coroutine frame.  It does so by altering
   operator new() to provide minimum possible alignment and then checking
   whether, despite that, an automatic storage duration variable moved into the
   frame is correctly aligned.  */

static std::unordered_map<void*, std::ptrdiff_t> off_map;
static std::unordered_map<void*, std::size_t> osz_map;

#pragma GCC diagnostic ignored "-Wpointer-arith"

static constexpr auto overalignment = 2 * __STDCPP_DEFAULT_NEW_ALIGNMENT__;

struct task
{
  struct promise_type
  {
    promise_type()
    {}

    task get_return_object () noexcept {
      return {std::coroutine_handle<promise_type>::from_promise (*this)};
    }
    void unhandled_exception () noexcept {}
    std::suspend_never initial_suspend () { return {}; }
    std::suspend_never final_suspend () noexcept { return {}; }
    void return_void () {}

    void
    operator delete (void* ptr, std::size_t sz)
    {
      auto off = off_map.at (ptr);
      auto osz = osz_map.at (ptr);
      assert (osz == sz);
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
      osz_map.emplace (x, sz);
      return x;
    }
  };
  std::coroutine_handle<promise_type> ch;
};

static bool ov_allocd = false;

struct alignas (overalignment) overaligned
{
  overaligned()
  {
    auto ithis = reinterpret_cast<std::uintptr_t> (this);
    if (ithis % overalignment)
      std::terminate ();
    ov_allocd = true;
  }
};

task
foo ()
{
  overaligned ov_var;
  co_await std::suspend_always{};
  co_return;
}

int
main ()
{
  auto x = foo ();
  x.ch ();

  assert (off_map.empty ());

  /* See note above.  */
  assert (ov_allocd);
}

// Ensure that we're actually testing something.
// { dg-final { scan-lang-dump "\n_Z.*\\.Frame\\s*\\{\[^\}\]*ov_var\[^\}\]*\\}" coro } }
