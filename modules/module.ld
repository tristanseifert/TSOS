OUTPUT_FORMAT(elf32-i386)

/* General data */
SECTIONS {
	.text ALIGN(4K): {
		*(.module_init);
		*(.text*);

		/* Initialisers */
		*(.init*);
		*(.fini*);

		/* Read-only data (strings, etc) */
		*(.rodata*);

		/* C++ destructor/constructor info */
		*(.ctors*);
		*(.dtors*);

		*(.jcr*);
	}

	/* Read/write data */
	.data : {
		*(.data*);
	}

	/* Version information */
	.info : {
		*(.info);
	}

	/* BSS */
	.bss ALIGN(4K): {
		*(.bss);
		*(COMMON);
	}

	/* Get rid of stuff we don't care for */
	/DISCARD/ : {
		*(.comment);
		*(.group);
	}
}