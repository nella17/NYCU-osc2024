#pragma once
#include "string.hpp"
#include "util.hpp"

extern bool mini_uart_use_async;

char mini_uart_getc_raw();
char mini_uart_getc();
void mini_uart_putc_raw(char c);
void mini_uart_putc(char c);

void mini_uart_setup();

char mini_uart_getc_raw_sync();
char mini_uart_getc_sync();
void mini_uart_putc_raw_sync(char c);
void mini_uart_putc_sync(char c);

void mini_uart_puts(const char* s);
int mini_uart_getline_echo(char* buffer, int size);
int PRINTF_FORMAT(1, 2) mini_uart_printf(const char* format, ...);
void mini_uart_print_hex(string_view view);
void mini_uart_print_str(string_view view);
void mini_uart_print(string_view view);
