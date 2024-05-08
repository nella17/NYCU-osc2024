#pragma once

#include "mm/mm.hpp"

constexpr uint64_t USER_STACK_SIZE = PAGE_SIZE;

extern "C" {
// exec.S
void exec_user_prog(void* user_text, void* user_stack, void* kernel_stack);
}

// TODO: handle argv
int exec(const char* name, char* const argv[]);
void exec_new_user_prog(void* ctx);
