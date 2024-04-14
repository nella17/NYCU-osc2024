#include "board/mailbox.hpp"
#include "board/mini-uart.hpp"
#include "shell/cmd.hpp"

int cmd_hwinfo(int /* argc */, char* /* argv */[]) {
  // it should be 0xa020d3 for rpi3 b+
  mini_uart_printf("Board revision :\t0x%08X\n", get_board_revision());
  mini_uart_printf("ARM memory base:\t0x%08X\n", get_arm_memory(0));
  mini_uart_printf("ARM memory size:\t0x%08X\n", get_arm_memory(1));
  return 0;
}
