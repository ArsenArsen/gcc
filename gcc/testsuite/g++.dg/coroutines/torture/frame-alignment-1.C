// { dg-do run }
#include <cstdint>
#include <cassert>
#include <exception>
#include <coroutine>
#include <unordered_map>

/* This test checks that our synthesized coroutine allocation correctly aligns
   the promise type inside the coroutine frame.  It does so by altering
   operator new() to provide minimum possible alignment and then checking
   whether, despite that, promise_type has a well-allocaed 'this'.  Since it
   was convenient, it also verifies that we deallocate all addresses we
   allocate.  */

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

    task get_return_object () noexcept { return {}; }
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
};

task
foo ()
{
  co_return;
}

int
main ()
{
  foo ();

  assert (off_map.empty ());
}
