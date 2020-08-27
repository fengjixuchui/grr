/*
 * GRR hv stage
 */
#include <stddef.h>
#include <stdint.h>
#include "uart.h"
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

	uart_setup();
	uart_print("GRR hv stage entry\n");

	/* Print memory map */
	for (mmap_i = 0; mmap_i < handover->mmap_entries; ++mmap_i)
		uart_print("%p-%p %d\n",
			handover->mmap[mmap_i].addr,
			handover->mmap[mmap_i].addr
			+ handover->mmap[mmap_i].size - 1,
			handover->mmap[mmap_i].type);

	for (;;)
		;
}
