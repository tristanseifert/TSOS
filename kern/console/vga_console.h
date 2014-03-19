// Colour values
enum vga_colour {
	vga_colour_black = 0,
	vga_colour_blue = 1,
	vga_colour_green = 2,
	vga_colour_cyan = 3,
	vga_colour_red = 4,
	vga_colour_magenta = 5,
	vga_colour_brown = 6,
	vga_colour_light_grey = 7,
	vga_colour_dark_grey = 8,
	vga_colour_light_blue = 9,
	vga_colour_light_green = 10,
	vga_colour_light_cyan = 11,
	vga_colour_light_red = 12,
	vga_colour_light_magenta = 13,
	vga_colour_light_brown = 14,
	vga_colour_white = 15,
};

// Compile in various optional things
#define VGA_USE_BOLD_FONT	1
#define VGA_USE_90x30_MODE	1

// Initialise VGA driver
void vga_init(void);

// Updates the internal VGA memory pointer address
void vga_textmem_remap(unsigned int newaddr);

// Print a character to the VGA console
void vga_console_putchar(char c);
void vga_console_reset();

// Manipulate VGA colours
enum vga_colour vga_get_fg_colour(void);
void vga_set_fg_colour(enum vga_colour colour);
enum vga_colour vga_get_bg_colour(void);
void vga_set_bg_colour(enum vga_colour colour);