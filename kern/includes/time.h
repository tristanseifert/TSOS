typedef struct time_components time_components_t;

struct time_components {
	unsigned int year, month, day;
	unsigned int hour, minute, second;
};

typedef unsigned int time_t;

// Date/time conversion
char *time_components_to_string(time_components_t c);