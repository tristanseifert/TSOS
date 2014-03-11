#import <types.h>

static const char month_map[16][4] = {
	"Jan",
	"Feb", 
	"Mar",
	"Apr",
	"May",
	"Jun",
	"Jul",
	"Aug",
	"Sep",
	"Oct",
	"Nov",
	"Dec"
};

/*
 * Converts time components to a string representation
 */
char *time_components_to_string(time_components_t c) {
	char *out = (char *) kmalloc(64);

	snprintf(out, 64, "%s %02u, %u at %02u:%02u:%02u", month_map[c.month], c.day,
		c.year, c.hour, c.minute, c.second);

	return out;
}