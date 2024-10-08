#pragma once

#include "board/timer.hpp"
#include "ds/list.hpp"
#include "ds/timeval.hpp"

struct Timer : ListItem<Timer> {
  using fp = void (*)(void*);
  uint64_t tick;
  int prio;
  fp callback;
  void* context;
  Timer()
      : ListItem{}, tick(0), prio(-1), callback(nullptr), context(nullptr) {}
  Timer(uint64_t tick_, int prio_, fp callback_, void* context_)
      : ListItem{},
        tick(tick_),
        prio(prio_),
        callback(callback_),
        context(context_) {}
};

constexpr uint64_t S_TO_US = 1e6;
extern uint64_t freq_of_timer, boot_timer_tick;
extern ListHead<Timer*> timers;
// cmds/timer.cpp
extern bool show_timer;
extern int timer_delay;

inline timeval tick2timeval(uint64_t tick) {
  uint32_t sec = tick / freq_of_timer;
  uint32_t usec = tick * S_TO_US / freq_of_timer % S_TO_US;
  return {sec, usec};
}

inline uint64_t timeval2tick(timeval tval) {
  return tval.sec * freq_of_timer + tval.usec * freq_of_timer / S_TO_US;
}

void timer_init();

inline void enable_timer() {
  set_core0_timer_irq_ctrl(true, 1);
}
inline void disable_timer() {
  set_core0_timer_irq_ctrl(false, 1);
}

inline void set_timer_tick(uint64_t tick) {
  write_sysreg(CNTP_CVAL_EL0, tick);
}

inline uint64_t get_timetick() {
  return read_sysreg(CNTPCT_EL0);
}

inline uint64_t get_current_tick() {
  return get_timetick() - boot_timer_tick;
}
inline timeval get_current_time() {
  return tick2timeval(get_current_tick());
}

void add_timer(timeval tval, void* context, Timer::fp callback, int prio = 0);
void add_timer(uint64_t tick, void* context, Timer::fp callback, int prio = 0);

void timer_enqueue();
