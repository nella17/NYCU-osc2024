#include "board/mini-uart.hpp"

#include "board/gpio.hpp"
#include "board/mmio.hpp"
#include "board/peripheral.hpp"
#include "interrupt.hpp"
#include "irq.hpp"
#include "nanoprintf.hpp"
#include "ringbuffer.hpp"
#include "timer.hpp"
#include "util.hpp"

decltype(&mini_uart_getc_raw) mini_uart_getc_raw_fp;
decltype(&mini_uart_getc) mini_uart_getc_fp;
decltype(&mini_uart_putc_raw) mini_uart_putc_raw_fp;
decltype(&mini_uart_putc) mini_uart_putc_fp;

#define RECEIVE_INT  0
#define TRANSMIT_INT 1

bool _mini_uart_is_async;
const bool& mini_uart_is_async = _mini_uart_is_async;
RingBuffer rbuf, wbuf;
int mini_uart_delay = 0;

namespace getline_echo {
bool enable = false;
char* buffer;
int length, r;

void impl(decltype(&mini_uart_putc_raw) putc,
          decltype(&mini_uart_getc_raw) getc) {
  auto c = getc();
  if (c == '\r' or c == '\n') {
    putc('\r');
    putc('\n');
    buffer[r] = '\0';
    enable = false;
  } else {
    switch (c) {
      case 8:     // ^H
      case 0x7f:  // backspace
        if (r > 0) {
          buffer[r--] = 0;
          putc('\b');
          putc(' ');
          putc('\b');
        }
        break;
      case 0x15:  // ^U
        while (r > 0) {
          buffer[r--] = 0;
          putc('\b');
          putc(' ');
          putc('\b');
        }
        break;
      case '\t':  // skip \t
        break;
      default:
        if (r + 1 < length) {
          buffer[r++] = c;
          putc(c);
        }
    }
  }
}

};  // namespace getline_echo

void set_ier_reg(bool enable, int bit) {
  SET_CLEAR_BIT(enable, AUX_MU_IER_REG, bit);
}

void mini_uart_use_async(bool use) {
  save_DAIF_disable_interrupt();

  _mini_uart_is_async = use;
  if (use) {
    set_aux_irq(true);
    set_ier_reg(true, RECEIVE_INT);
    // clear FIFO
    set32(AUX_MU_IER_REG, 3 << 1);
    mini_uart_getc_raw_fp = mini_uart_getc_raw_async;
    mini_uart_getc_fp = mini_uart_getc_async;
    mini_uart_putc_raw_fp = mini_uart_putc_raw_async;
    mini_uart_putc_fp = mini_uart_putc_async;
  } else {
    // TODO: handle rbuf / wbuf
    set_aux_irq(false);
    set_ier_reg(false, RECEIVE_INT);
    set_ier_reg(false, TRANSMIT_INT);
    mini_uart_getc_raw_fp = mini_uart_getc_raw_sync;
    mini_uart_getc_fp = mini_uart_getc_sync;
    mini_uart_putc_raw_fp = mini_uart_putc_raw_sync;
    mini_uart_putc_fp = mini_uart_putc_sync;
  }

  restore_DAIF();
}

void mini_uart_enqueue() {
  auto iir = get32(AUX_MU_IIR_REG);
  if (iir & (1 << 0))
    return;
  set_ier_reg(false, RECEIVE_INT);
  set_ier_reg(false, TRANSMIT_INT);
  irq_add_task(9, mini_uart_handler, (void*)(uint64_t)iir);
}

void mini_uart_handler(void* iir_) {
  auto iir = (uint32_t)(uint64_t)iir_;

  if (iir & (1 << 1)) {
    // Transmit holding register empty
    if (not wbuf.empty())
      set32(AUX_MU_IO_REG, wbuf.pop());
  } else if (iir & (1 << 2)) {
    // Receiver holds valid byte
    rbuf.push(get32(AUX_MU_IO_REG) & MASK(8));
  }

  if (mini_uart_delay > 0) {
    int delay = mini_uart_delay;
    mini_uart_delay = 0;
    mini_uart_printf_sync("delay uart %ds\n", delay);
    auto cur = get_current_tick();
    while (get_current_tick() - cur < delay * freq_of_timer)
      NOP;
  }

  if (getline_echo::enable and not rbuf.empty()) {
    using namespace getline_echo;
    auto putc = [](char c) { wbuf.push(c); };
    auto getc = []() { return rbuf.pop(true); };
    impl(putc, getc);
  }

  if (not wbuf.empty())
    set_ier_reg(true, TRANSMIT_INT);
  if (not rbuf.full())
    set_ier_reg(true, RECEIVE_INT);
}

char mini_uart_getc_raw_async() {
  auto c = rbuf.pop(true);
  set_ier_reg(true, RECEIVE_INT);
  return c;
}

void mini_uart_putc_raw_async(char c) {
  set_ier_reg(true, TRANSMIT_INT);
  wbuf.push(c, true);
}

char mini_uart_getc_raw_sync() {
  while ((get32(AUX_MU_LSR_REG) & 1) == 0)
    NOP;
  return get32(AUX_MU_IO_REG) & MASK(8);
}

void mini_uart_putc_raw_sync(char c) {
  while ((get32(AUX_MU_LSR_REG) & (1 << 5)) == 0)
    NOP;
  if (mini_uart_is_async) {
    // clear FIFO
    set32(AUX_MU_IER_REG, 3 << 1);
  }
  set32(AUX_MU_IO_REG, c);
}

void mini_uart_setup() {
  unsigned int r = get32(GPFSEL1);
  // set gpio14
  r = (r & ~(7 << 12)) | (GPIO_FSEL_ALT5 << 12);
  // set gpio15
  r = (r & ~(7 << 15)) | (GPIO_FSEL_ALT5 << 15);
  set32(GPFSEL1, r);

  // disable pull-up / down
  set32(GPPUD, 0b00);
  // wait 150 cycle
  wait_cycle(150);
  // set cycle for gpio14 & gpio15
  set32(GPPUDCLK0, get32(GPPUDCLK0) | (1 << 14) | (1 << 15));
  // wait 150 cycle
  wait_cycle(150);
  // remove the control signal
  set32(GPPUD, 0);
  // remove clock
  set32(GPPUDCLK0, 0);

  // enable mini uart
  set32(AUX_ENABLES, get32(AUX_ENABLES) | 1);
  // disable tx & rx
  set32(AUX_MU_CNTL_REG, 0);
  // disable interrupt
  set32(AUX_MU_IER_REG, 0);
  // set data size to 8 bit
  set32(AUX_MU_LCR_REG, 3);
  // no auto flow control
  set32(AUX_MU_MCR_REG, 0);
  // baud rate 115200
  set32(AUX_MU_BAUD_REG, 270);
  // no fifo
  set32(AUX_MU_IIR_REG, 6);
  // enable tx & rx
  set32(AUX_MU_CNTL_REG, 3);

  mini_uart_use_async(false);
}

char mini_uart_getc_async() {
  char c = mini_uart_getc_raw_async();
  return c == '\r' ? '\n' : c;
}

char mini_uart_getc_sync() {
  char c = mini_uart_getc_raw_sync();
  return c == '\r' ? '\n' : c;
}

void mini_uart_putc_async(char c) {
  if (c == '\n')
    mini_uart_putc_raw_async('\r');
  mini_uart_putc_raw_async(c);
}

void mini_uart_putc_sync(char c) {
  if (c == '\n')
    mini_uart_putc_raw_sync('\r');
  mini_uart_putc_raw_sync(c);
}

char mini_uart_getc_raw() {
  return mini_uart_getc_raw_fp();
}
char mini_uart_getc() {
  return mini_uart_getc_fp();
}
void mini_uart_putc_raw(char c) {
  return mini_uart_putc_raw_fp(c);
}
void mini_uart_putc(char c) {
  return mini_uart_putc_fp(c);
}

int mini_uart_getline_echo(char* buffer, int length) {
  if (length <= 0)
    return -1;

  using namespace getline_echo;

  getline_echo::buffer = buffer;
  getline_echo::length = length;
  enable = true;
  r = 0;

  if (mini_uart_is_async)
    while (enable)
      NOP;
  else
    while (enable)
      impl(&mini_uart_putc_raw_sync, &mini_uart_getc_raw_sync);

  return r;
}

void mini_uart_npf_putc(int c, void* /* ctx */) {
  mini_uart_putc(c);
}

int mini_uart_printf(const char* format, ...) {
  va_list args;
  va_start(args, format);
  int size = npf_vpprintf(&mini_uart_npf_putc, NULL, format, args);
  va_end(args);
  return size;
}

void mini_uart_npf_putc_sync(int c, void* /* ctx */) {
  mini_uart_putc_sync(c);
}

int mini_uart_printf_sync(const char* format, ...) {
  va_list args;
  va_start(args, format);
  int size = npf_vpprintf(&mini_uart_npf_putc, NULL, format, args);
  va_end(args);
  return size;
}

void mini_uart_puts(const char* s) {
  for (char c; (c = *s); s++)
    mini_uart_putc(c);
}

void mini_uart_puts_sync(const char* s) {
  for (char c; (c = *s); s++)
    mini_uart_putc_sync(c);
}

void mini_uart_print_hex(string_view view) {
  for (auto c : view)
    mini_uart_printf("%02x", c);
}
void mini_uart_print_str(string_view view) {
  for (auto c : view)
    mini_uart_putc(c);
}

void mini_uart_print(string_view view) {
  bool printable = true;
  for (int i = 0; i < view.size(); i++) {
    auto c = view[i];
    printable &= (0x20 <= c and c <= 0x7e) or (i + 1 == view.size() and c == 0);
  }
  if (printable)
    mini_uart_print_str(view);
  else
    mini_uart_print_hex(view);
}
