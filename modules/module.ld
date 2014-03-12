OUTPUT_FORMAT(elf32-i386)

/* General data */
SECTIONS {
	.text ALIGN(4K): {
		*(.text);

		*(.rodata);
		*(.rodata*);

		/* Initialisers */
		*(.init);
		*(.fini);

		/* C++ destructor/constructor info */
		*(.ctors);
		*(.dtors);

		/* Some other initialisation */
		*(.jcr);
	}

	/* Read/write data */
	.data : {
		*(.data);
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
		*(.eh_frame);
	}
}