# HCI-PicoW-C-RawSPI
HCI PicoW layer in C using raw SPI access to the CYW43439 chip in the Pico W - so this is the SPI layer below the HCI layer.   

For completeness, the HCI layer is used in ```HCI-PicoW-C``` and the functions found in the CYW43 driver (https://github.com/georgerobotics/cyw43-driver/tree/main), in ``` cyw43_ctrl.c```.

This code uses the existing functions to set up the CWY43439 chip - because this uses undocumented features and addresses, and reverse engineering would take a long time for minimal benefit.   

After set-up, it then uses the ```cyw43_spi_transfer``` function found in the pico-sdk (https://github.com/raspberrypi/pico-sdk/blob/master/src/rp2_common/pico_cyw43_driver/cyw43_bus_pio_spi.c). This uses PIO to talk directly to the CYW43439 using the half-duplex SPI channel between the two chips.   
