/*
 * Platform bus, containing devices in the computer that aren't on PnP busses,
 * or ones that can be enumerated. Includes stuff like chipset peripherals
 * like the KBC.
 *
 * Note that matches on drivers are based on the devices' names.
 */
#import <types.h>
#import "bus.h"

extern bus_t *platform_bus;