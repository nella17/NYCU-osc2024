#pragma once

#include "timeval.hpp"
#include "util.hpp"

extern uint64_t freq_of_timer, boot_timer_tick;
extern bool show_timer;

uint64_t get_timetick();
timeval tick2timeval(uint64_t tick);

void core_timer_enable();
void set_timer(int second);
timeval get_boot_time();

void timer_handler();
