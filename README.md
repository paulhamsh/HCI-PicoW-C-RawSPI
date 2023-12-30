# HCI-PicoW-C-RawSPI
HCI PicoW layer in C using raw SPI access to the CYW43439 chip in the Pico W - so this is the SPI layer below the HCI layer.   

For completeness, the HCI layer is used in ```HCI-PicoW-C``` and the functions found in the CYW43 driver (https://github.com/georgerobotics/cyw43-driver/tree/main), in ``` cyw43_ctrl.c```.

This code uses the existing functions to set up the CWY43439 chip - because this uses undocumented features and addresses, and reverse engineering would take a long time for minimal benefit.   

After set-up, it then uses the ```cyw43_spi_transfer``` function found in the pico-sdk (https://github.com/raspberrypi/pico-sdk/blob/master/src/rp2_common/pico_cyw43_driver/cyw43_bus_pio_spi.c). This uses PIO to talk directly to the CYW43439 using the half-duplex SPI channel between the two chips.   

Using use SPI directly bypasses a lot of code in the CYW43 driver, the shared bus driver, the bluetooth code and in BT Stack.  The main change required in ```cyw43_bus_pio_spi.c``` are to the use of ```cyw43_int_t *self```.    To remove the need for this in each function, a local copy is made in ```cyw43_spi_init``` and then used in each function after that.   This means changing code in the pico-sdk source tree and all the changes are documented at the start of the ```pico_cyw.c``` file.   The functions impacted are in the list below.   Also need to remove any ```inline```.    

```
                          cyw43_int_t *save_self = NULL;

cyw43_spi_init            saved_self = self;
cyw43_deinit              if (self == NULL) self = saved_self;
                          saved_self = NULL;

start_spi_comms           if (self == NULL) self = saved_self;

cyw43_spi_transfer        if (self == NULL) self = saved_self;
_cyw43_read_reg           if (self == NULL) self = saved_self; 
_cyw43_write_reg          if (self == NULL) self = saved_self;
read_reg_u32_swap         if (self == NULL) self = saved_self;
write_reg_u32_swap        if (self == NULL) self = saved_self;
cyw43_read_bytes          if (self == NULL) self = saved_self;
cyw43_write_bytes         if (self == NULL) self = saved_self;
```
