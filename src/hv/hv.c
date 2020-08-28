/*
 * GRR hv stage
 */
#include <stddef.h>
#include <stdint.h>
#include "uart.h"
#include "bootparam.h"
#include "../handover.h"

static
uint64_t
gdt[] = {
	0,
	0,
	0x00209A0000000000,	/* __BOOT_CS */
	0x0000920000000000,	/* __BOOT_DS */
};

static
void
load_gdt(void)
{
	struct {
		uint16_t limit;
		uint64_t addr;
	} __attribute__((packed)) gdtr;

	gdtr.limit = sizeof(gdt);
	gdtr.addr = (uint64_t) gdt;

	asm volatile ("lgdt %0\n"
			"pushq $0x10\n"
			"pushq $reload_cs\n"
			"retfq; reload_cs:\n"
			"movl $0x18, %%eax\n"
			"movl %%eax, %%ds\n"
			"movl %%eax, %%es\n"
			"movl %%eax, %%ss\n"
			"movl %%eax, %%fs\n"
			"movl %%eax, %%gs"
			 :: "m" (gdtr) : "rax");
}

void
grr_hv_main(grr_handover *handover)
{
	size_t mmap_i;

	/*
	 * It would be great to not have this on the stack, but
	 * that's the only safe storage we have before the kernel,
	 * initrd and cmdline we're memmove'd to their final locations
	 */
	struct setup_header setup;

	uart_setup();
	uart_print("GRR hv stage entry\n");

	/* Print memory map */
	uart_print("Memory map:\n");
	for (mmap_i = 0; mmap_i < handover->mmap_entries; ++mmap_i)
		uart_print("\t%d\t%p-%p\n",
			handover->mmap[mmap_i].type,
			handover->mmap[mmap_i].addr,
			handover->mmap[mmap_i].addr
			+ handover->mmap[mmap_i].size - 1);

	/* Print the kernel and initrd we got */
	uart_print("Kernel at: %p (%d bytes)\n",
		handover->linux, handover->linux_size);
	uart_print("Initrd at: %p (%d bytes)\n",
		handover->initrd, handover->initrd_size);
	uart_print("Command line: %s\n", handover->cmdline);

	memmove(&setup, handover->linux + 0x1f1, sizeof(struct setup_header));
	if (setup.boot_flag != 0xaa55 ||
			setup.header != 0x53726448 ||
			setup.version < 0x02c ||
			!setup.relocatable_kernel ||
			!(setup.xloadflags & XLF_KERNEL_64) ||
			!(setup.xloadflags & XLF_CAN_BE_LOADED_ABOVE_4G)) {
		uart_print("Kernel not supported by loader!\n");
		return;
	}
}
