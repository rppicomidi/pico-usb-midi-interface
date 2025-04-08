/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2023 rppicomidi
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>

#include "bsp/board.h"
#include "tusb.h"
#include "pio_midi_uart_lib.h"
#include "midi_uart_lib.h"
#include "midi_device_multistream.h"
#include "cdc_stdio_lib.h"
#include "embedded_cli.h"
//--------------------------------------------------------------------+
// This program routes 5-pin DIN MIDI IN signals A & B to USB MIDI
// virtual cables 0 & 1 on the USB MIDI Bulk IN endpoint. It also
// routes MIDI data from USB MIDI virtual cables 0-5 on the USB MIDI
// Bulk OUT endpoint to the 5-pin DIN MIDI OUT signals A-F.
// The Pico board's LED blinks in a pattern depending on the Pico's
// USB connection state (See below).
//--------------------------------------------------------------------+


//--------------------------------------------------------------------+
// MACRO CONSTANT TYPEDEF PROTYPES
//--------------------------------------------------------------------+

/* Blink pattern
 * - 250 ms  : device not mounted
 * - 1000 ms : device mounted
 * - 2500 ms : device is suspended
 */
enum  {
  BLINK_NOT_MOUNTED = 250,
  BLINK_MOUNTED = 1000,
  BLINK_SUSPENDED = 2500,
};
#if NUM_PIOS == 2
#define NUM_PIO_MIDI_UARTS 4
#else
#define NUM_PIO_MIDI_UARTS 6
#endif
#define NUM_HW_MIDI_UARTS 2

static uint32_t blink_interval_ms = BLINK_NOT_MOUNTED;

static void led_blinking_task(void);
static void midi_task(void);
static void cli_task(void);
static void cli_init(void);
static void printWelcome(void);

static void* pio_midi_uarts[NUM_PIO_MIDI_UARTS]; // MIDI IN A-F and MIDI OUT A-F
static void* hw_midi_uarts[NUM_HW_MIDI_UARTS];
// PIO MIDI UART pin usage (Move them if you want to)
static const uint MIDI_OUT_A_GPIO = 2;
static const uint MIDI_IN_A_GPIO = 3;
static const uint MIDI_OUT_B_GPIO = 6;
static const uint MIDI_IN_B_GPIO = 7;
static const uint MIDI_OUT_C_GPIO = 8;
static const uint MIDI_IN_C_GPIO = 9;
static const uint MIDI_OUT_D_GPIO = 10;
static const uint MIDI_IN_D_GPIO = 11;
#if NUM_PIOS > 2
static const uint MIDI_OUT_E_GPIO = 12;
static const uint MIDI_IN_E_GPIO = 13;
static const uint MIDI_OUT_F_GPIO = 14;
static const uint MIDI_IN_F_GPIO = 15;
#endif
// HW MIDI UART pin usage (Move them if you want to,
// but make sure the right UARTs can be mapped to the pins) 
static const uint MIDI_OUT_G_GPIO = 4;
static const uint MIDI_IN_G_GPIO = 5;
static const uint HW_MIDI_UART_G = 1; // hardware UART Number 0 or 1
#if NUM_HW_MIDI_UARTS > 1
static const uint MIDI_OUT_H_GPIO = 0;
static const uint MIDI_IN_H_GPIO = 1;
static const uint HW_MIDI_UART_H = 0; // hardware UART Number 0 or 1
#endif
#define MAX_PORT_NAME 12
typedef struct {
  uint8_t pio_uart_number_list[NUM_PIO_MIDI_UARTS];
  uint8_t hw_uart_number_list[NUM_HW_MIDI_UARTS];
  uint8_t usb_midi_cable_list[CFG_TUD_MIDI_NUMCABLES_OUT];
  uint8_t num_pio_uart_routes;
  uint8_t num_hw_uart_routes;
  uint8_t num_usb_midi_routes;
} midi_input_routes_t;
static midi_input_routes_t usb_routes[CFG_TUD_MIDI_NUMCABLES_IN];
static midi_input_routes_t pio_routes[NUM_PIO_MIDI_UARTS];
static midi_input_routes_t hw_routes[NUM_HW_MIDI_UARTS];
static volatile bool cdc_state_has_changed = false;
static volatile bool cli_up_message_pending = false;
static absolute_time_t previous_timestamp;
static EmbeddedCli* cli;
void init_routes()
{
  memset(usb_routes, 0, sizeof(usb_routes));
  memset(pio_routes, 0, sizeof(pio_routes));
  memset(hw_routes, 0, sizeof(hw_routes));

  for (size_t idx=0; idx < NUM_PIO_MIDI_UARTS; idx++) {
    usb_routes[idx].pio_uart_number_list[0] = idx +'A';
    usb_routes[idx].num_pio_uart_routes = 1;
    pio_routes[idx].usb_midi_cable_list[0] = idx + '1';
    pio_routes[idx].num_usb_midi_routes = 1;
  }
  for (size_t idx=0; idx < NUM_HW_MIDI_UARTS; idx++) {
    size_t jdx = idx + NUM_PIO_MIDI_UARTS;
    usb_routes[jdx].hw_uart_number_list[0] = idx + 'G';
    usb_routes[jdx].num_hw_uart_routes = 1;
    hw_routes[idx].usb_midi_cable_list[0] = jdx + '1';
    hw_routes[idx].num_usb_midi_routes = 1;
  }
}

bool is_port_valid(uint8_t port)
{
  bool valid = false;
  if (port >= '1' && port <= '8') {
    valid = (port-'1') < CFG_TUD_MIDI_NUMCABLES_IN;
  }
  else {
    port = toupper(port);
    valid = (port >='A' && port <='F' && (port-'A') < NUM_PIO_MIDI_UARTS) ||
            port == 'G' || port == 'H';
  }
  return valid;
}

static bool route(midi_input_routes_t* route, uint8_t out)
{
  if (out <='8') {
    for (size_t idx=0; idx < route->num_usb_midi_routes; idx++) {
      if (route->usb_midi_cable_list[idx] == out) {
        return true; // already in the list
      }
    }
    if (route->num_usb_midi_routes >= CFG_TUD_MIDI_NUMCABLES_IN) {
      return false; // should never happen
    }
    route->usb_midi_cable_list[route->num_usb_midi_routes++]=out;
  }
  else if (out <='F') {
    for (size_t idx=0; idx < route->num_pio_uart_routes; idx++) {
      if (route->pio_uart_number_list[idx] == out) {
        return true; // already in the list
      }
    }
    if (route->num_pio_uart_routes >= NUM_PIO_MIDI_UARTS) {
      return false; // should never happen
    }
    route->pio_uart_number_list[route->num_pio_uart_routes++]=out;
  }
  else if (out <='H') {
    for (size_t idx=0; idx < route->num_hw_uart_routes; idx++) {
      if (route->hw_uart_number_list[idx] == out) {
        return true; // already in the list
      }
    }
    if (route->num_hw_uart_routes >= NUM_HW_MIDI_UARTS) {
      return false; // should never happen
    }
    route->hw_uart_number_list[route->num_hw_uart_routes++]=out;
  }
  else {
    return false;
  }
  return true; // successfully added the route 
}

static bool unroute(midi_input_routes_t* route, uint8_t out)
{
  if (out <='8') {
    for (size_t idx=0; idx < route->num_usb_midi_routes; idx++) {
      if (route->usb_midi_cable_list[idx] == out) {
        route->usb_midi_cable_list[idx] = route->usb_midi_cable_list[route->num_usb_midi_routes--];
        return true;
      }
    }
  }
  else if (out <='F') {
    for (size_t idx=0; idx < route->num_pio_uart_routes; idx++) {
      if (route->pio_uart_number_list[idx] == out) {
        route->pio_uart_number_list[idx] = route->pio_uart_number_list[route->num_pio_uart_routes--];
        return true;
      }
    }
  }
  else if (out <='H') {
    for (size_t idx=0; idx < route->num_hw_uart_routes; idx++) {
      if (route->hw_uart_number_list[idx] == out) {
        route->hw_uart_number_list[idx] = route->hw_uart_number_list[route->num_hw_uart_routes--];
        return true;
      }
    }
  }
  return false; // not routed
}

static bool is_routed(midi_input_routes_t* route, uint8_t out)
{
  if (out <='8') {
    for (size_t idx=0; idx < route->num_usb_midi_routes; idx++) {
      if (route->usb_midi_cable_list[idx] == out) {
        return true;
      }
    }
  }
  else if (out <='F') {
    for (size_t idx=0; idx < route->num_pio_uart_routes; idx++) {
      if (route->pio_uart_number_list[idx] == out) {
        return true;
      }
    }
  }
  else if (out <='H') {
    for (size_t idx=0; idx < route->num_hw_uart_routes; idx++) {
      if (route->hw_uart_number_list[idx] == out) {
        return true;
      }
    }
  }
  return false; // not routed
}
/**
 * @brief route the MIDI in to MIDI out
 * 
 * @param in: '1'-'8' are USB cable numbers 0-7; 'A'-'F' ar PIO UARTS; 'G'-'H' are HW UARTS
 * @param out: '1'-'8' are USB cable numbers 0-7; 'A'-'F' ar PIO UARTS; 'G'-'H' are HW UARTS
 */
bool connect(uint8_t in, uint8_t out)
{
  bool result = is_port_valid(in) && is_port_valid(out);
  if (result) {
    if (in <= '8') {
      result = route(usb_routes + (in-'1'), out);
    }
    else if (in <= 'F') {
      result = route(pio_routes + (in - 'A'), out);
    }
    else {
      result = route(hw_routes + (in-'G'), out);
    }
  }
  return result;
}

bool disconnect(uint8_t in, uint8_t out)
{
  bool result = is_port_valid(in) && is_port_valid(out);
  if (result) {
    if (in <= '8') {
      result = unroute(usb_routes + (in-'1'), out);
    }
    else if (in <= 'F') {
      result = unroute(pio_routes + (in - 'A'), out);
    }
    else {
      result = unroute(hw_routes + (in-'G'), out);
    }
  }
  return result;
}

bool is_connected(uint8_t in, uint8_t out)
{
  bool result = is_port_valid(in) && is_port_valid(out);
  if (result) {
    if (in <= '8') {
      result = is_routed(usb_routes + (in-'1'), out);
    }
    else if (in <= 'F') {
      result = is_routed(pio_routes + (in - 'A'), out);
    }
    else {
      result = is_routed(hw_routes + (in-'G'), out);
    }
  }
  return result;
}

/*------------- MAIN -------------*/
int main(void)
{
  board_init();
  init_routes();
  // init device stack on configured roothub port
  tud_init(BOARD_TUD_RHPORT);
  cdc_stdio_lib_init();
  cli_init();
  // Create the MIDI UARTs and MIDI OUTs
  pio_midi_uarts[0] = pio_midi_uart_create(MIDI_OUT_A_GPIO, MIDI_IN_A_GPIO);
  assert(pio_midi_uarts[0] != NULL);
  pio_midi_uarts[1] = pio_midi_uart_create(MIDI_OUT_B_GPIO, MIDI_IN_B_GPIO);
  assert(pio_midi_uarts[1] != NULL);
  pio_midi_uarts[2] = pio_midi_uart_create(MIDI_OUT_C_GPIO, MIDI_IN_C_GPIO);
  assert(pio_midi_uarts[2] != NULL);
  pio_midi_uarts[3] = pio_midi_uart_create(MIDI_OUT_D_GPIO, MIDI_IN_D_GPIO);
  assert(pio_midi_uarts[3] != NULL);
  #if NUM_PIOS > 2
  pio_midi_uarts[4] = pio_midi_uart_create(MIDI_OUT_E_GPIO, MIDI_IN_E_GPIO);
  assert(pio_midi_uarts[4] != NULL);
  pio_midi_uarts[5] = pio_midi_uart_create(MIDI_OUT_F_GPIO, MIDI_IN_F_GPIO);
  assert(pio_midi_uarts[5] != NULL);
  #endif
  hw_midi_uarts[0] = midi_uart_configure(HW_MIDI_UART_G, MIDI_OUT_G_GPIO, MIDI_IN_G_GPIO);
  assert(hw_midi_uarts[0] != NULL);
  #if NUM_HW_MIDI_UARTS > 1
  hw_midi_uarts[1] = midi_uart_configure(HW_MIDI_UART_H, MIDI_OUT_H_GPIO, MIDI_IN_H_GPIO);
  assert(hw_midi_uarts[0] != NULL);
  #endif

#if NUM_PIOS > 2
  printf("8-IN 8-OUT USB MIDI Device adapter\r\n");
#else
  printf("6-IN 6-OUT USB MIDI Device adapter\r\n");
#endif
  // 
  while (1)
  {
    tud_task(); // tinyusb device task
    led_blinking_task();
    midi_task();
    cli_task();
    if (cli_up_message_pending)
    {
      absolute_time_t now = get_absolute_time();

      int64_t diff = absolute_time_diff_us(previous_timestamp, now);
      if (diff > 1000000ll) {
        cli_up_message_pending = false;
        printWelcome();
      }
    }
  }
}

//--------------------------------------------------------------------+
// Device callbacks
//--------------------------------------------------------------------+

// Invoked when device is mounted
void tud_mount_cb(void)
{
  blink_interval_ms = BLINK_MOUNTED;
}

// Invoked when device is unmounted
void tud_umount_cb(void)
{
  blink_interval_ms = BLINK_NOT_MOUNTED;
  cli_up_message_pending = false;
  cdc_state_has_changed = false;
}

// Invoked when usb bus is suspended
// remote_wakeup_en : if host allow us  to perform remote wakeup
// Within 7ms, device must draw an average of current less than 2.5 mA from bus
void tud_suspend_cb(bool remote_wakeup_en)
{
  (void) remote_wakeup_en;
  blink_interval_ms = BLINK_SUSPENDED;
}

// Invoked when usb bus is resumed
void tud_resume_cb(void)
{
  blink_interval_ms = BLINK_MOUNTED;
}

//--------------------------------------------------------------------+
// MIDI Task
//--------------------------------------------------------------------+
static void send_to_connected(midi_input_routes_t* routes, uint8_t *rx, uint8_t nread, bool connected)
{
  if (nread > 0)
  {
    for (size_t cable = 0; connected && cable < routes->num_usb_midi_routes; cable++) {
      uint32_t nwritten = tud_midi_stream_write(routes->usb_midi_cable_list[cable] - '1', rx, nread);
      if (nwritten != nread) {
        TU_LOG1("Warning: Dropped %lu bytes sending to port %c\r\n", nread - nwritten, routes->usb_midi_cable_list[cable]);
      }
    }
    for (size_t idx = 0; idx < routes->num_pio_uart_routes; idx++) {
      uint32_t nwritten = pio_midi_uart_write_tx_buffer(pio_midi_uarts[routes->pio_uart_number_list[idx] - 'A'], rx, nread);
      if (nwritten != nread) {
        TU_LOG1("Warning: Dropped %lu bytes sending to port %c\r\n", nread - nwritten, routes->pio_uart_number_list[idx]);
      }
    }
    for (size_t idx = 0; idx < routes->num_hw_uart_routes; idx++) {
      uint32_t nwritten = midi_uart_write_tx_buffer(hw_midi_uarts[routes->hw_uart_number_list[idx] - 'G'], rx, nread);
      if (nwritten != nread) {
        TU_LOG1("Warning: Dropped %lu bytes sending to port %c\r\n", nread - nwritten, routes->hw_uart_number_list[idx]);
      }
    }
  }
}
static void poll_midi_uarts_rx(bool connected)
{
  uint8_t rx[48];
  // Pull any bytes received on the MIDI UARTs out of the receive buffers and
  // send them out via USB MIDI on the corresponding virtual cable
  for (uint8_t idx = 0; idx < NUM_PIO_MIDI_UARTS; idx++) {
    uint8_t nread = pio_midi_uart_poll_rx_buffer(pio_midi_uarts[idx], rx, sizeof(rx));
    send_to_connected(pio_routes+idx, rx, nread, connected);
  }
  for (uint8_t idx = 0; idx < NUM_HW_MIDI_UARTS; idx++) {
    uint8_t nread = midi_uart_poll_rx_buffer(hw_midi_uarts[idx], rx, sizeof(rx));
    send_to_connected(hw_routes+idx, rx, nread, connected);
  }    
}

static void poll_usb_rx(bool connected)
{
    // device must be attached and have the endpoint ready to receive a message
    if (!connected)
    {
        return;
    }
    uint8_t rx[48];
    uint8_t cable_num;
    uint32_t nread =  tud_midi_demux_stream_read(&cable_num, rx, sizeof(rx));
    while (nread > 0) {
      send_to_connected(usb_routes+cable_num,rx, nread, connected);
      nread =  tud_midi_demux_stream_read(&cable_num, rx, sizeof(rx));
    }
}

static void drain_serial_port_tx_buffers()
{
    uint8_t cable;
    for (cable = 0; cable < NUM_PIO_MIDI_UARTS; cable++) {
        pio_midi_uart_drain_tx_buffer(pio_midi_uarts[cable]);
    }
    for (cable = 0; cable < NUM_HW_MIDI_UARTS; cable++) {
        midi_uart_drain_tx_buffer(hw_midi_uarts[cable]);
    }
}
static void midi_task(void)
{
    bool connected = tud_midi_mounted();
    poll_midi_uarts_rx(connected);
    poll_usb_rx(connected);
    drain_serial_port_tx_buffers();
}

//--------------------------------------------------------------------+
// BLINKING TASK
//--------------------------------------------------------------------+
static void led_blinking_task(void)
{
  static uint32_t start_ms = 0;
  static bool led_state = false;

  // Blink every interval ms
  if ( board_millis() - start_ms < blink_interval_ms) return; // not enough time
  start_ms += blink_interval_ms;

  board_led_write(led_state);
  led_state = 1 - led_state; // toggle
}

//--------------------------------------------------------------------+
// CLI TASK
//--------------------------------------------------------------------+
static void cli_task(void)
{
  if (cdc_state_has_changed) {
    cdc_state_has_changed = false;
    cli_up_message_pending = tud_cdc_connected();
    previous_timestamp = get_absolute_time();
  }
  int c = getchar_timeout_us(0);
  if (c != PICO_ERROR_TIMEOUT)
  {
    embeddedCliReceiveChar(cli, c);
    embeddedCliProcess(cli);
  }
}
/*
* The following 3 functions are required by the EmbeddedCli library
*/
static void onCommand(const char *name, char *tokens)
{
  printf("Received command: %s\r\n", name);

  for (int i = 0; i < embeddedCliGetTokenCount(tokens); ++i)
  {
    printf("Arg %d : %s\r\n", i, embeddedCliGetToken(tokens, i + 1));
  }
}

static void onCommandFn(EmbeddedCli *embeddedCli, CliCommand *command)
{
  (void)embeddedCli;
  embeddedCliTokenizeArgs(command->args);
  onCommand(command->name == NULL ? "" : command->name, command->args);
}

static void writeCharFn(EmbeddedCli *embeddedCli, char c)
{
  (void)embeddedCli;
  putchar(c);
}

static void show_connected_row(int in)
{
  for (int cable = 0; cable < CFG_TUD_MIDI_NUMCABLES_OUT; cable++) {
    char connection_mark = is_connected(in, cable+'1') ? 'X' : ' ';
    printf(" %c |", connection_mark);
  }
  for (int cable = 0; cable < NUM_PIO_MIDI_UARTS; cable++) {
    char connection_mark = is_connected(in, cable+'A') ? 'X' : ' ';
    printf(" %c |", connection_mark);
  }
  for (int cable = 0; cable < NUM_HW_MIDI_UARTS; cable++) {
    char connection_mark = is_connected(in, cable+'G') ? 'X' : ' ';
    printf(" %c |", connection_mark);
  }
  printf("\r\n------------+");
  for (size_t col = 0; col < CFG_TUD_MIDI_NUMCABLES_IN+NUM_PIO_MIDI_UARTS+NUM_HW_MIDI_UARTS; col++)
  {
    printf("---+");
  }
  printf("\r\n");
}

static void showFn(EmbeddedCli *cli, char *args, void *context)
{
  (void)cli;
  (void)args;
  (void)context;
  // Print the top header
  for (size_t line = 0; line < 12; line++)
  {
    if (line == 0)
        printf("        TO->|");
    else if (line == 7)
        printf("  FROM |    |");
    else if (line == 8)
        printf("       v    |");
    else
        printf("            |");

    for (size_t idx = 0; idx < CFG_TUD_MIDI_NUMCABLES_IN; idx++)
    {
      char nickname[] = "   USB OUT 1";
      nickname[11] += idx;
      printf(" %c |", nickname[line]);
    }
    for (size_t idx = 0; idx < NUM_PIO_MIDI_UARTS; idx++)
    {
      char nickname[] = "SERIAL OUT A";
      nickname[11] += idx;
      printf(" %c |", nickname[line]);
    }
    for (size_t idx = 0; idx < NUM_HW_MIDI_UARTS; idx++)
    {
      char nickname[] = "SERIAL OUT G";
      nickname[11] += idx;
      printf(" %c |", nickname[line]);
    }
    printf("\r\n");
  }
  printf("------------+");
  for (size_t col = 0; col < CFG_TUD_MIDI_NUMCABLES_IN+NUM_PIO_MIDI_UARTS+NUM_HW_MIDI_UARTS; col++)
  {
    printf("---+");
  }
  printf("\r\n");

  for (size_t idx = 0; idx < CFG_TUD_MIDI_NUMCABLES_IN; idx++)
  {
    char nickname[] = "    USB IN 1";
    nickname[11] += idx;
    printf("%-12s|", nickname);
    show_connected_row(idx+'1');
  }
  for (size_t idx = 0; idx < NUM_PIO_MIDI_UARTS; idx++)
  {
    char nickname[] = " SERIAL IN A";
    nickname[11] += idx;
    printf("%-12s|", nickname);
    show_connected_row(idx+'A');
  }
  for (size_t idx = 0; idx < NUM_HW_MIDI_UARTS; idx++)
  {
    char nickname[] = " SERIAL IN G";
    nickname[11] += idx;
    printf("%-12s|", nickname);
    show_connected_row(idx+'G');
  }
}

void print_port_id_description(void)
{
  printf("The single character port ID to use in commands can be\r\n"
    "1-%d for USB MIDI ",CFG_TUD_MIDI_NUMCABLES_IN);
#if (NUM_PIO_MIDI_UARTS  == 4)
  printf("and can be A-D, G-H for Serial MIDI\r\n");
#else
  printf("\tand can be A-H for Serial MIDI \r\n");
#endif
}
void print_port_range_error_message(const char* src, char port)
{
#if NUM_PIO_MIDI_UARTS == 6
  printf("%s %c not valid. Can be 1-%d or A-H\r\n", src, port, CFG_TUD_MIDI_NUMCABLES_IN);
#else
  printf("%s %c not valid. Can be 1-%d or A-D, G-H\r\n", src, port, CFG_TUD_MIDI_NUMCABLES_IN);
#endif
}
void connectFn(EmbeddedCli *cli, char *args, void *context)
{
  (void)cli;
  (void)context;
  if (embeddedCliGetTokenCount(args) != 2) {
    printf("connect <FROM port ID> <TO port ID>\r\n");
    return;
  }
  const char* from = embeddedCliGetToken(args, 1);
  const char* to = embeddedCliGetToken(args, 2);
  if (!is_port_valid(*from)) {
    print_port_range_error_message("From Input", *from);
  }
  else if (!is_port_valid(*to)) {
    print_port_range_error_message("To Output", *to);
  }
  else if (!connect(*from, *to)) {
    printf("Connect from %c to %c failed\r\n",*from, *to);
  }
  else {
    printf("Connected %c to %c\r\n", *from, *to);
  }
}

void disconnectFn(EmbeddedCli *cli, char *args, void *context)
{
  (void)cli;
  (void)context;
  if (embeddedCliGetTokenCount(args) != 2) {
    printf("disconnect <FROM port ID> <TO port ID>\r\n");
    return;
  }
  const char* from = embeddedCliGetToken(args, 1);
  const char* to = embeddedCliGetToken(args, 2);
  if (!is_port_valid(*from)) {
    print_port_range_error_message("From Input", *from);
  }
  else if (!is_port_valid(*to)) {
    print_port_range_error_message("To Output", *to);
  }
  else if (!disconnect(*from, *to)) {
    printf("Disconnect from %c to %c failed\r\n",*from, *to);
  }
  else {
    printf("Disconnected %c from %c\r\n", *from, *to);
  }
}

static void cli_init(void)
{
  EmbeddedCliConfig cli_config = {
    .invitation = "> ",
    .rxBufferSize = 64,
    .cmdBufferSize = 64,
    .historyBufferSize = 128,
    .maxBindingCount = 10,
    .cliBuffer = NULL,
    .cliBufferSize = 0,
    .enableAutoComplete = true,
  };
  cli = embeddedCliNew(&cli_config);
  cli->onCommand = onCommandFn;
  cli->writeChar = writeCharFn;
  CliCommandBinding cmd;
  cmd.name = "connect";
  cmd.help = "Route a MIDI stream. usage connect <From port ID> <To port ID>";
  cmd.tokenizeArgs = true;
  cmd.context = NULL;
  cmd.binding = connectFn;
  volatile bool result = embeddedCliAddBinding(cli, cmd);
  assert(result);
  cmd.name = "disconnect";
  cmd.help = "Unroute a MIDI stream. usage disconnect <From port ID> <To port ID>";
  cmd.tokenizeArgs = true;
  cmd.context = NULL;
  cmd.binding = disconnectFn;
  result = embeddedCliAddBinding(cli, cmd);
  assert(result);
  cmd.name = "show";
  cmd.help = "Show MIDI stream routing. usage: show";
  cmd.tokenizeArgs = false;
  cmd.context = NULL;
  cmd.binding = showFn;
  result = embeddedCliAddBinding(cli, cmd);
  assert(result);

  (void)result;
}

void printWelcome(void)
{
    printf("\r\n\r\n");
    printf("Cli is running.\r\n");
    printf("Type \"help\" for a list of commands\r\n");
    printf("Use backspace and tab to remove chars and autocomplete\r\n");
    printf("Use up and down arrows to recall previous commands\r\n\r\n");
    print_port_id_description();
    embeddedCliReceiveChar(cli, '\r');
    embeddedCliProcess(cli);
}

// Invoked when cdc when line state changed e.g connected/disconnected
void tud_cdc_line_state_cb(uint8_t itf, bool dtr, bool rts)
{
  (void) itf;

  // connected
  if ( dtr && rts )
  {
    cdc_state_has_changed = true;
  }
}

// Invoked when CDC interface received data from host
void tud_cdc_rx_cb(uint8_t itf)
{
  (void) itf;
}
