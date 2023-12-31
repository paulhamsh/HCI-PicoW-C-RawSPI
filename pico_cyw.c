/*
 * pico_cyw.c
 *
 * Program to use HCI functions for Pico Pi W and also test direct SPI access
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "bsp/board.h"
#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"


// #defines for this code


#define BUF_SIZE 200
#define PRE_BUFFER_LEN 3

// global variables

bool take_over = false;

// Two example HCI commands for testing with

uint8_t hci_cmd1[] = {0x01, 0x03, 0x0C, 0x00};
uint8_t hci_cmd2[] = {0x01, 0x01, 0x10, 0x00};

// Used for SPI packet dumps - to use must #define DUMP_SPI_TRANSACTIONS in cyw43_bus_pio_spi.c

extern bool enable_spi_packet_dumping;

// Function to print a buffer in hex

void print_data(char *pre_text, uint8_t *data, uint32_t len) {
    printf("%s (size %lu): ", pre_text, (unsigned long) len);
    for (int i = 0; i < len; i++) {
        if (i % 8 == 0) printf("\n    %02x: ", i);
        printf("%02x ", data[i]);
    };
    printf("\n");
}


/*
 * HCI command functions
 */

// Handler for activity on the cyw43 driver
// Called from cyw43_ctrl.c from cyw43_poll_func()
// Will print out a packet received unless take_over is true, in which case nothing is done

void cyw43_bluetooth_hci_process(void) {
    uint8_t hci_packet[BUF_SIZE];
    uint32_t hci_len = 0;
    bool has_work;
    if (!take_over) {   // only do a hci_read if we haven't taken over the HCI interactions
        do {
            int err = cyw43_bluetooth_hci_read(hci_packet, sizeof(hci_packet), &hci_len);
            if (err == 0 && hci_len > 0) {
                printf("GOT A PACKET %lu\n", (unsigned long) hci_len);
                print_data("HCI process", hci_packet, hci_len);
                has_work = true;
            } else {
                has_work = false;
            }
        } while (has_work);
    };
}

// These functions add or remove the pre-buffer of three bytes, to make this the raw HCI packet

uint32_t hci_receive_raw(uint8_t *data, uint32_t max_len) {
    int ret;
    uint32_t buf_len;
    uint8_t buf[BUF_SIZE];

    ret = cyw43_bluetooth_hci_read(buf, max_len, &buf_len);
    if (ret) buf_len = 0;

    // Remove the padding
    if (buf_len > PRE_BUFFER_LEN) buf_len -= PRE_BUFFER_LEN;
    memcpy(data, &buf[PRE_BUFFER_LEN], buf_len);

    return buf_len;
}

void hci_send_raw(uint8_t *data, uint32_t data_len) {
    uint8_t buf[BUF_SIZE];

    memcpy(&buf[PRE_BUFFER_LEN], data, data_len);
    cyw43_bluetooth_hci_write(buf, data_len + PRE_BUFFER_LEN);
}


/*
 * SPI functions
 */

#define BACKPLANE_FUNC  1
#define SPI_FUNC  0
#define BACK_PADDING 16

uint32_t buf_base, send_head, send_tail, receive_head, receive_tail;

static inline uint32_t make_cmd(bool write, bool inc, uint32_t fn, uint32_t addr, uint32_t sz) {
    return write << 31 | inc << 30 | fn << 28 | (addr & 0x1ffff) << 11 | sz;
}

// external function definitions - from cyw43_bus_pio_spi.c - but with first parameter set to void *

extern int      cyw43_spi_transfer(void *self, const uint8_t *tx, size_t tx_length, uint8_t *rx, size_t rx_length);

extern uint32_t _cyw43_read_reg(void *self, uint32_t fn, uint32_t reg, uint size);
extern int      _cyw43_write_reg(void *self, uint32_t fn, uint32_t reg, uint32_t val, uint size);

extern uint32_t cyw43_read_reg_u32(void *self, uint32_t fn, uint32_t reg);
extern int      cyw43_read_reg_u16(void *self, uint32_t fn, uint32_t reg);
extern int      cyw43_read_reg_u8(void *self, uint32_t fn, uint32_t reg);

extern int      cyw43_write_reg_u32(void *self, uint32_t fn, uint32_t reg, uint32_t val);
extern int      cyw43_write_reg_u16(void *self, uint32_t fn, uint32_t reg, uint16_t val);
extern int      cyw43_write_reg_u8(void *self, uint32_t fn, uint32_t reg, uint32_t val);

extern uint32_t read_reg_u32_swap(void *self, uint32_t fn, uint32_t reg);
extern int      write_reg_u32_swap(void *self, uint32_t fn, uint32_t reg, uint32_t val);

extern int      cyw43_read_bytes(void *self, uint32_t fn, uint32_t addr, size_t len, uint8_t *buf);
extern int      cyw43_write_bytes(void *self, uint32_t fn, uint32_t addr, size_t len, const uint8_t *src);



void write_backplane_value(uint32_t val) {
    _cyw43_write_reg(&cyw43_state, BACKPLANE_FUNC, 0x1000a, (val & 0xff00) >> 8, 1);
    _cyw43_write_reg(&cyw43_state, BACKPLANE_FUNC, 0x1000b, (val & 0xff0000) >> 16, 1);
    _cyw43_write_reg(&cyw43_state, BACKPLANE_FUNC, 0x1000c, (val & 0xff000000) >> 24, 1);
}


void read_buffer_pointers() {
    //  ensure backplane window is for the buffer area
    send_head = _cyw43_read_reg(&cyw43_state, BACKPLANE_FUNC, buf_base + 0x2000 , 4);
    send_tail = _cyw43_read_reg(&cyw43_state, BACKPLANE_FUNC, buf_base + 0x2004 , 4);
    printf("Send buffer:    head %lx tail %lx\n", send_head, send_tail);

    receive_head = _cyw43_read_reg(&cyw43_state, BACKPLANE_FUNC, buf_base + 0x2008 , 4);
    receive_tail = _cyw43_read_reg(&cyw43_state, BACKPLANE_FUNC, buf_base + 0x200c , 4);
    printf("Receive buffer: head %x tail %lx\n", receive_head, receive_tail);
}

int main() {
    uint32_t bufsend[50];
    uint32_t val;
    uint8_t *buffer;

    enable_spi_packet_dumping = false;

    stdio_init_all();

    for (int i=0; i < 10; i++) {
        printf("%i\n", i);
        sleep_ms(1000);
    }
    printf("Started\n");

    if (cyw43_arch_init()) return -1;
    cyw43_init(&cyw43_state);
    cyw43_bluetooth_hci_init();

    sleep_ms(1000);

    // Some use of HCI functions to test they work

    hci_send_raw(hci_cmd1, 4);
    print_data("Sent", hci_cmd1, 4);

    sleep_ms(1000);

    //enable_spi_packet_dumping = true;

    hci_send_raw(hci_cmd2, 4);
    print_data("Sent", hci_cmd2, 4);

    sleep_ms(1000);


    //  And now use SPI layer directly

    take_over = true; // stop read on polling
    buffer = (uint8_t *)bufsend;


    printf("Starting direct access to SPI\n");
    printf("-----------------------------\n");

    // Check the FEEDBEAD register

    val = _cyw43_read_reg(&cyw43_state, SPI_FUNC, 0x14, 8);
    printf("Test data %lx\n", val);

    // Now read bus register

    bufsend[0] = make_cmd(false, true, SPI_FUNC, 0x00, 32);
    cyw43_spi_transfer(&cyw43_state,  NULL, 4, buffer,  32);
    print_data("SPI registers", &buffer[4], 32);

    // Set backplane to chipset
    write_backplane_value(0x18000000);

    // Make sure it worked
    val = _cyw43_read_reg(&cyw43_state, BACKPLANE_FUNC, 0x1000a, 3);
    printf("Backplane %lx\n", val);

    // Obtain base address for buffers
    printf("Command for base address %lx\n", make_cmd(false, true, BACKPLANE_FUNC, 0x0d68, 3));
    buf_base = _cyw43_read_reg(&cyw43_state, BACKPLANE_FUNC, 0x0d68, 3);
    printf("Buffer base address %lx\n", buf_base);

    // Set backplane to buffers and read pointers
    write_backplane_value(buf_base);
    read_buffer_pointers();

    // Now read pointers as a memory read
    bufsend[0] = make_cmd(false, true, BACKPLANE_FUNC, buf_base + 0x2000, 16);
    cyw43_spi_transfer(&cyw43_state,  NULL, 4, buffer, BACK_PADDING + 4 + 16);
    print_data("Ring buffer pointers", &buffer[20], 16);

    // Change receive tail
    _cyw43_write_reg(&cyw43_state, BACKPLANE_FUNC, buf_base + 0x200c, receive_tail, 4);


    // Write data to send ring buffer
    bufsend[0] = make_cmd(true, true, BACKPLANE_FUNC, buf_base + send_head, 8);
    uint8_t data[] = {0x03, 0x00, 0x00, 0x01, 0x03, 0x0c, 0x00, 0x00};
    memcpy(&buffer[4], data, 8);
    cyw43_spi_transfer(&cyw43_state, buffer, 12, NULL, 0);

    // Update send head
    _cyw43_write_reg(&cyw43_state, BACKPLANE_FUNC, buf_base + 0x2000,  send_head + 8, 4);

    // And check it
    send_head = _cyw43_read_reg(&cyw43_state, BACKPLANE_FUNC, buf_base + 0x2000 , 4);
    printf("Send head %lx\n", send_head);

    // And get the pointers again
    bufsend[0] = make_cmd(false, true, BACKPLANE_FUNC, buf_base + 0x2000, 16);
    cyw43_spi_transfer(&cyw43_state,  NULL, 4, buffer, BACK_PADDING + 4 + 16);
    print_data("Ring buffer pointers", &buffer[20], 16);

    // Show data data we wrote
    bufsend[0] = make_cmd(false, true, BACKPLANE_FUNC, buf_base + 0x0000, 32);
    cyw43_spi_transfer(&cyw43_state,  NULL, 4, buffer, BACK_PADDING + 4 + 32);
    print_data("Ring buffer data out", &buffer[20], 32);

    // Toggle data ready flag to send to HCI layer
    write_backplane_value(0x18000000);
    val = _cyw43_read_reg(&cyw43_state, BACKPLANE_FUNC, 0x18000d6c, 4);
    val = val ^ 2;
    _cyw43_write_reg(&cyw43_state, BACKPLANE_FUNC, 0x18000d6c, val, 4);

    sleep_ms(500);

    // Read buffer pointers
    write_backplane_value(buf_base);
    // And get the pointers again
    bufsend[0] = make_cmd(false, true, BACKPLANE_FUNC, buf_base + 0x2000, 16);
    cyw43_spi_transfer(&cyw43_state,  NULL, 4, buffer, BACK_PADDING + 4 + 16);
    print_data("Ring buffer pointers", &buffer[20], 16);
    read_buffer_pointers();
    // Read buffer pointers

    // Read receive buffer data
    bufsend[0] = make_cmd(false, true, BACKPLANE_FUNC, buf_base + 0x1000, 64);
    cyw43_spi_transfer(&cyw43_state,  NULL, 4, buffer, BACK_PADDING + 4 + 64);
    print_data("Ring buffer data in ", &buffer[20], 64);


    enable_spi_packet_dumping = false;

    while (1)  {
        sleep_ms(500);
        cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, 1);
        sleep_ms(500);
        cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, 0);
    };
    cyw43_deinit(&cyw43_state);

    printf("Finished\n");
    return 0;
}
