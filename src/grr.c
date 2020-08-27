/*
 * GRR main file
 */

#include <efi.h>
#include <efiutil.h>
#include "uart.h"

efi_status
boot_linux(efi_ch16 *filename, const char *cmdline);

void
efiapi
efi_main(efi_handle image_handle, efi_system_table *system_table)
{
	efi_status status;

	/* Setup efiutil globals */
	init_util(image_handle, system_table);

	/* Setup serial port */
	uart_setup();
	uart_print("Hello, World!\n");

	status = boot_linux(L"\\efi\\grr\\vmlinuz-4.19.0-10-amd64", "");
	abort(L"Failed to boot Linux!\r\n", status);
}
