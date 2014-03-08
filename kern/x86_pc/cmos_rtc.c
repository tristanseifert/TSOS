#import <types.h>
#import "bus/bus.h"
#import "runtime/rand.h"

#import "cmos_rtc.h"
#import "interrupts.h"

#define DRIVER_NAME "CMOS Clock"

#define CMOS_REG_PORT	0x70
#define CMOS_DATA_PORT	0x71

// Private functions
static void* rtc_init(device_t *dev);
static bool rtc_match(device_t *dev);
static void rtc_sys_tick(void* ctx);

static void rtc_read(void);

// RTC access helpers
static bool rtc_update_in_progress();
static uint8_t rtc_read_reg(uint8_t reg);

// Driver definition
static const driver_t driver = {
	.name = DRIVER_NAME,
	.supportsDevice = rtc_match,
	.getDriverData = rtc_init
};

// Interrupt counter (rate is 2Hz, we want 1Hz)
static volatile uint32_t tick;

// Note that internally, time is represented as 24 hours.
static struct {
	unsigned int hour;
	unsigned int minute;
	unsigned int second;

	unsigned int day_of_month;

	unsigned int day;
	unsigned int month;
	unsigned int year;
} time;

/*
 * Register the driver.
 */
static int rtc_driver_register(void) {
	bus_register_driver((driver_t *) &driver, PLATFORM_BUS_NAME);
	return 0;
}

module_driver_init(rtc_driver_register);

/*
 * All devices that match DRIVER_NAME will work under this driver.
 */
static bool rtc_match(device_t *dev) {
	if(strcmp(dev->node.name, DRIVER_NAME) == 0) return true;

	return false;
}

/*
 * Initialises the CMOS RTC driver.
 */
static void* rtc_init(device_t *dev) {
	// Read initial RTC values
	rtc_read();

	// Enable the periodic IRQ
	IRQ_OFF();

	io_outb(CMOS_REG_PORT, 0x8B);
	uint8_t reg = io_inb(CMOS_DATA_PORT);

	io_outb(CMOS_REG_PORT, 0x8B);
	io_outb(CMOS_DATA_PORT, reg | 0x40);

	// Rate is 2 Hz
	io_outb(CMOS_REG_PORT, 0x8A);
	reg = io_inb(CMOS_DATA_PORT);

	io_outb(CMOS_REG_PORT, 0x8A);
	io_outb(CMOS_DATA_PORT, (reg & 0xF0) | 0x0F);

	// Install IRQ handler
	irq_register_handler(8, rtc_sys_tick, NULL);

	// Re-enable IRQs
	IRQ_RES();

	return NULL;
}

/*
 * Reads the date and time from the RTC. This is kind of slow.
 */
static void rtc_read(void) {
	uint8_t century = 0;
	uint8_t last_second;
	uint8_t last_minute;
	uint8_t last_hour;
	uint8_t last_day;
	uint8_t last_month;
	uint8_t last_year;
	uint8_t last_century;
	uint8_t registerB;
 
	while (rtc_update_in_progress());
	time.second = rtc_read_reg(0x00);
	time.minute = rtc_read_reg(0x02);
	time.hour = rtc_read_reg(0x04);
	time.day = rtc_read_reg(0x07);
	time.month = rtc_read_reg(0x08);
	time.year = rtc_read_reg(0x09);

	// Read century
	century = rtc_read_reg(0x32);
 
 	// Repeat the RTC read process until the values are consistent
	do {
		last_second = time.second;
		last_minute = time.minute;
		last_hour = time.hour;
		last_day = time.day;
		last_month = time.month;
		last_year = time.year;
		last_century = century;
 
 		// Wait for there to not be an update
		while(rtc_update_in_progress());

		time.second = rtc_read_reg(0x00);
		time.minute = rtc_read_reg(0x02);
		time.hour = rtc_read_reg(0x04);
		time.day = rtc_read_reg(0x07);
		time.month = rtc_read_reg(0x08);
		time.year = rtc_read_reg(0x09);

		century = rtc_read_reg(0x32);
	} while((last_second != time.second) || (last_minute != time.minute) || (last_hour != time.hour) ||
		   (last_day != time.day) || (last_month != time.month) || (last_year != time.year) ||
		   (last_century != century));
 
 	// Read register B (determine if 24 hour time)
	registerB = rtc_read_reg(0x0B);
 
	// Convert BCD to binary values if necessary
	if (!(registerB & 0x04)) {
		time.second = (time.second & 0x0F) + ((time.second / 16) * 10);
		time.minute = (time.minute & 0x0F) + ((time.minute / 16) * 10);
		time.hour = ((time.hour & 0x0F) + (((time.hour & 0x70) / 16) * 10) ) | (time.hour & 0x80);
		time.day = (time.day & 0x0F) + ((time.day / 16) * 10);
		time.month = (time.month & 0x0F) + ((time.month / 16) * 10);
		time.year = (time.year & 0x0F) + ((time.year / 16) * 10);

		century = (century & 0x0F) + ((century / 16) * 10);
	}
 
	// Convert 12 hour clock to 24 hour clock if necessary
	if (!(registerB & 0x02) && (time.hour & 0x80)) {
		time.hour = ((time.hour & 0x7F) + 12) % 24;
	}
 
	// Calculate the full (4-digit) year
	time.year += century * 100;
}

/*
 * Checks whether an RTC update is in progress
 */
static bool rtc_update_in_progress() {
	io_outb(CMOS_REG_PORT, 0x0A);
	return io_inb(CMOS_DATA_PORT) & 0x80;
}

/*
 * Reads a particular RTC register
 */
static uint8_t rtc_read_reg(uint8_t reg) {
	io_outb(CMOS_REG_PORT, reg);
	return io_inb(CMOS_DATA_PORT);
}

/*
 * IRQ handler for the RTC (called twice a second)
 */
static void rtc_sys_tick(void* ctx) {
	// Read the RTC register so IRQ will happen again
	io_outb(0x70, 0x0C);
	io_inb(0x71);

	// Only increment time every second
	if(tick++ == 2) {
		tick = 0;
	} else {
		return;
	}

	// Increment seconds
	if(time.second++ == 59) {
		time.second = 0;

		// Increment minutes
		if(time.minute++ == 59) {
			time.minute = 0;

			// Resync with hardware every hour
			rtc_read();
		}
	}

	// Reseed PRNG
	srand(irq_count());

	// klog(kLogLevelDebug, "%02u:%02u:%02u (%02u-%02u-%04u)", time.hour, time.minute, time.second, time.day, time.month, time.year);
}