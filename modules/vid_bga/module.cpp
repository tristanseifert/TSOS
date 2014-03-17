#import <module.h>

// Initialisers
extern "C" void _init(void);

// Module definition
static const module_t mod = {
	/*.name = */ MODULE_NAME
};

/*
 * Initialisation function called by kernel
 */
extern "C" {
	 __attribute__ ((section (".module_init"))) module_t *start(void) {
		// Call constructors and whatnot
		_init();

		KDEBUG("pls bochs");

		return (module_t *) &mod;
	}
}
