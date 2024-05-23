#pragma once

#include "ds/list.hpp"
#include "ds/mem.hpp"
#include "mm/mm.hpp"
#include "mm/mmu.hpp"
#include "sched.hpp"
#include "signal.hpp"
#include "util.hpp"

constexpr uint64_t KTHREAD_STACK_SIZE = PAGE_SIZE;

struct KthreadItem : ListItem {
  struct Kthread* thread;
  KthreadItem(Kthread* th) : ListItem{}, thread{th} {}
};

enum class KthreadStatus {
  kNone = 0,
  kReady,
  kRunning,
  kWaiting,
  kDead,
};

struct Kthread : ListItem {
  Regs regs;

  using fp = void (*)(void*);
  int tid;
  KthreadStatus status = KthreadStatus::kReady;
  int exit_code = 0;
  Mem kernel_stack;
  KthreadItem* item;
  Signal signal;
  PT* el0_tlb;

 private:
  Kthread();
  friend void kthread_init();

 public:
  Kthread(Kthread::fp start, void* ctx);
  Kthread(const Kthread& o);
  ~Kthread();

  void fix(const Kthread& o, Mem& mem);
  void fix(const Kthread& o, void* faddr, uint64_t fsize);
  void* fix(const Kthread& o, void* ptr);
  void ensure_el0_tlb();
  int alloc_user_pages(uint64_t va, uint64_t size, ProtFlags prot);
  template <typename T, typename U>
  int map_user_phy_pages(T va, U pa, uint64_t size, ProtFlags prot) {
    return map_user_phy_pages_impl((uint64_t)va, (uint64_t)pa, size, prot);
  }
  int map_user_phy_pages_impl(uint64_t va, uint64_t pa, uint64_t size,
                              ProtFlags prot);
  void reset_kernel_stack() {
    regs.sp = kernel_stack.end(0x10);
  }
  void reset_el0_tlb() {
    if (el0_tlb) {
      delete el0_tlb;
      el0_tlb = nullptr;
    }
  }
};

inline void set_current_thread(Kthread* thread) {
  write_sysreg(TPIDR_EL1, thread);
}
inline Kthread* current_thread() {
  return (Kthread*)read_sysreg(TPIDR_EL1);
}

extern ListHead<Kthread> kthreads;
void add_list(Kthread* thread);
void del_list(Kthread* thread);
Kthread* find_thread_by_tid(int tid);

void idle();
int new_tid();

void kthread_init();
void kthread_start();
void kthread_wait(int pid);
void kthread_kill(int pid);
void kthread_kill(Kthread* thread);
void kthread_exit(int status);
void kthread_fini();
Kthread* kthread_create(Kthread::fp start, void* ctx = nullptr);
long kthread_fork();
