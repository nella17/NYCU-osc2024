#pragma once

#include "ds/list.hpp"
#include "int/exception.hpp"

struct Kthread;

#define SIGKILL 9

constexpr int NSIG = 32;
using signal_handler = void (*)();

struct SigAction {
  bool in_kernel;
  signal_handler handler;
};

struct SignalItem : ListItem {
  int signal;
  SignalItem(int sig) : ListItem{}, signal(sig) {}
};

struct Signal {
  Kthread* cur;
  ListHead<SignalItem> list;
  SigAction actions[NSIG];

  Signal(Kthread*);
  Signal(Kthread*, const Signal& other);

  void regist(int signal, signal_handler handler);
  void operator()(int signal);
  void handle(TrapFrame* frame);
};

void signal_handler_nop();
void signal_handler_kill();

void signal_kill(int pid, int signal);
void signal_return(TrapFrame* frame);
void el0_sig_return();
