/*
 * Data that is passed from the efi to the hv stage
 */
#ifndef HANDOVER_H
#define HANDOVER_H

/*
 * Memory map entry type
 */
enum {
	/*
	 * The block is free to use
	 */
	GRR_E820_USABLE		= 1,
	/*
	 * The block is reserved by the platform
	 */
	GRR_E820_RESERVED	= 2,
	/*
	 * The block can be reclaimed after the tables inside it were parsed
	 */
	GRR_E820_ACPI_RECLAIM	= 3,
	/*
	 * ACPI non-volatile storage
	 */
	GRR_E820_APCI_NVS	= 4,
	/*
	 * The block cannot be used, might be faulty memory
	 */
	GRR_E820_UNUSABLE	= 5,
};

/*
 * Memory map entry
 */
typedef struct {
	/*
	 * Physical starting address of the block
	 */
	uint64_t	addr;
	/*
	 * Size of the block in bytes
	 */
	uint64_t	size;
	/*
	 * Type of the block
	 */
	uint32_t	type;
} grr_e820_entry;

/*
 * Video information
 */
typedef struct {
	/*
	 * Framebuffer base address
	 */
	void	*base;
	/*
	 * Framebuffer size
	 */
	size_t	size;
	/*
	 * Horizontal screen resolution
	 */
	size_t	width;
	/*
	 * Vertical resolution
	 */
	size_t	height;
	/*
	 * Pixel size in bytes
	 */
	size_t	depth;
	/*
	 * Size and position of each subpixel
	 */
	size_t	red_size, red_pos;
	size_t	green_size, green_pos;
	size_t	blue_size, blue_pos;
	size_t	rsvd_size, rsvd_pos;
	/*
	 * Line size in bytes
	 */
	size_t pitch;
} grr_video_info;

/*
 * System info handover block
 */
#define GRR_MMAP_MAX_ENTRIES 128
typedef struct {
	/*
	 * Number of entries in the memory map
	 */
	size_t		mmap_entries;
	/*
	 * Memory map
	 */
	grr_e820_entry	mmap[GRR_MMAP_MAX_ENTRIES];

	/*
	 * Video information
	 */
	grr_video_info	video_info;

	/*
	 * Pointer to the ACPI RSDP
	 */
	void		*acpi_rsdp;

	/*
	 * Pointer to the SMBIOS entry point table
	 */
	void		*smbios_ept;

	/*
	 * OS
	 */
	void		*linux;		/* Kernel buffer and size */
	size_t		linux_size;
	void		*initrd;	/* Initrd buffer and size */
	size_t		initrd_size;
	const char	*cmdline;	/* Kernel command line */
} grr_handover;

/*
 * The entry point of the hypervisor
 */
void
grr_hv_entry(grr_handover *handover);

#endif
