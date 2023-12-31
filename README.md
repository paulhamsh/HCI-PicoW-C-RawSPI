# HCI-PicoW-C-RawSPI
HCI PicoW layer in C using raw SPI access to the CYW43439 chip in the Pico W - so this is the SPI layer below the HCI layer.   

This code uses the existing functions to set up the CWY43439 chip - because this uses undocumented features and addresses, and reverse engineering would take a long time for minimal benefit.  These are

```
cyw43_arch_init();
cyw43_init(&cyw43_state);
cyw43_bluetooth_hci_init();  // loads the firmware straight away
```

After set-up, it then uses the ```cyw43_spi_transfer``` function found in the pico-sdk (pico-sdk/src/rp2_common/pico_cyw43_driver/cyw43_bus_pio_spi.c). This uses PIO to talk directly to the CYW43439 using the half-duplex SPI channel between the two chips.   

(For reference, HCI layer access is used in repo ```HCI-PicoW-C``` and this uses the functions found in the CYW43 driver (georgerobotics/cyw43-driver), in ```cyw43_ctrl.c```).   


## Use &cyw43_state as self ##

The calls to SPI functions need a ```self``` variable - this maps to a pointer to ```&cvyw43_state``` (as defined ```cyw43.h``` in georgerobotics/cyw43-driver) and is set in cyw43_ctrl.c in line 229:

```
cyw43_t *self = &cyw43_state;
```

The function calls in ```cyw43_bus_pio_spi.c``` actually use self as a pointer to cyw43_int_t (```cyw43_int_t *self```).  So I have changed them to ```void *``` in the interest of simplicity, but the function calls use ```&cyw43_state``` as the first parameter.   

## Printing debug information ##

Debugging can be set at three points in the CYW43439 code.


### 1 Shared bus driver ###

For debugging, cyw logs can be set like this.   
In ```pico-sdk/src/rp2_common/pico_cyw43_driver/cybt_shared_bus/cybt_shared_bus_driver.``` add
```
        #undef NDEBUG
        #define CYBT_DEBUG 1
```

### 2 SPI driver ###

SPI logs can be set like this.   
In ```pico-sdk/src/rp2_common/pico_cyw43_driver/cyw43_bus_pio_spi.c``` add

```
#define ENABLE_SPI_DUMPING 1
```

and change line 55 from:

```
static bool enable_spi_packet_dumping; // set to true to dump
```

to:
    
```
bool enable_spi_packet_dumping; // set to true to dump
```

so that it can be accessed from your program.   

### 3 CYW43 driver ###

Edit ``pico-sdk/lib/cyw43-driver/src/cyw43_config.h``` line 129 to add

```
#undef NDEBUG
```

Or edit the DEBUG definitions so that they print when you want them to.

```
#ifndef CYW43_PRINTF
#include <stdio.h>
#define CYW43_PRINTF(...) void(0)     
//#define CYW43_PRINTF(...) printf(__VA_ARGS__)
#endif

#ifndef CYW43_VDEBUG
#define CYW43_VDEBUG(...) (void)0
#define CYW43_VERBOSE_DEBUG 0
#endif

#ifndef CYW43_DEBUG
//#ifdef NDEBUG
//#define CYW43_DEBUG(...) (void)0
//#else
#define CYW43_DEBUG(...) CYW43_PRINTF(__VA_ARGS__)
//#endif
#endif

#ifndef CYW43_INFO
#define CYW43_INFO(...) CYW43_PRINTF(__VA_ARGS__)
#endif

#ifndef CYW43_WARN
#define CYW43_WARN(...) CYW43_PRINTF("[CYW43] " __VA_ARGS__)
#endif
```


## Amending code in cyw43_bus_pio_spi.c to NOT require self - deprecated ##

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
