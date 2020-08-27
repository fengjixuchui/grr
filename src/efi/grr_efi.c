/*
 * GRR efi stage
 */

#include <efi.h>
#include <efiutil.h>
#include "../handover.h"

/*
 * Load a file from disk into memory
 */
static
efi_status
load_file(efi_ch16 *path, efi_size *buffer_size, void **buffer)
{
	efi_status 		status;
	efi_file_protocol 	*file;
	efi_file_info 		*info;

	status = self_root_dir->open(
		self_root_dir,
		&file,
		path,
		EFI_FILE_MODE_READ,
		0);
	if (EFI_ERROR(status))
		return status;

	status = get_file_info(file, &info);
	if (EFI_ERROR(status))
		return status;

	*buffer = malloc((*buffer_size = info->file_size));
	status = file->read(file, buffer_size, *buffer);

	if (EFI_ERROR(status)) {
		free(*buffer);
		goto end;
	}

	if (*buffer_size != info->file_size) {
		status = EFI_LOAD_ERROR;
		free(*buffer);
		goto end;
	}

end:
	free(info);
	status = file->close(file);
	return status;
}

/*
 * Fill video info in the handover block
 */
static
efi_status
fill_video(grr_handover *handover)
{
	efi_status	status;
	efi_size	handle_cnt;
	efi_handle	*handles;

	efi_graphics_output_protocol *gop;
	efi_graphics_output_mode_information *mode_info;

	/* Locate all GOP hanldes */
	handles = NULL;
	status = locate_all_handles(
		&(efi_guid) EFI_GRAPHICS_OUTPUT_PROTOCOL_GUID,
		&handle_cnt,
		&handles);

	if (EFI_ERROR(status))
		goto end;

	if (!handle_cnt) {
		status = EFI_UNSUPPORTED;
		goto end;
	}

	/* We only care about the first GOP */
	status = bs->handle_protocol(
		handles[0],
		&(efi_guid) EFI_GRAPHICS_OUTPUT_PROTOCOL_GUID,
		(void **) &gop);

	if (EFI_ERROR(status))
		goto end;

	mode_info = gop->mode->info;
	switch (mode_info->pixel_format) {
	case pixel_red_green_blue_reserved_8_bit_per_color:
		handover->video_info.red_size = 8;
		handover->video_info.red_pos = 0;
		handover->video_info.green_size = 8;
		handover->video_info.green_pos = 8;
		handover->video_info.blue_size = 8;
		handover->video_info.blue_pos = 16;
		handover->video_info.rsvd_size = 8;
		handover->video_info.rsvd_pos = 24;
		break;
	case pixel_blue_green_red_reserved_8_bit_per_color:
				handover->video_info.red_size = 8;
		handover->video_info.red_pos = 16;
		handover->video_info.green_size = 8;
		handover->video_info.green_pos = 8;
		handover->video_info.blue_size = 8;
		handover->video_info.blue_pos = 0;
		handover->video_info.rsvd_size = 8;
		handover->video_info.rsvd_pos = 24;
		break;
	default: /* TODO: add more pixel formats */
		status = EFI_UNSUPPORTED;
		goto end;
	}

	handover->video_info.base = (void *) gop->mode->frame_buffer_base;
	handover->video_info.size = gop->mode->frame_buffer_size;
	handover->video_info.width = mode_info->horizontal_resolution;
	handover->video_info.height = mode_info->vertical_resolution;
	handover->video_info.depth = 32;
	handover->video_info.pitch = mode_info->pixels_per_scan_line * 4;

end:
	if (handles)
		free(handles);
	return status;
}

/*
 * Fill the memory map in the handover block
 */
static
efi_status
fill_mmap(grr_handover *handover, efi_size *map_key)
{
	efi_status status;

	/* UEFI memory map */
	void		*mmap;
	efi_size	mmap_size;
	efi_size	desc_size;
	efi_u32		desc_ver;
	efi_memory_descriptor *mmap_ent;

	/* E820 memory map */
	efi_size	e820_entries;
	grr_e820_entry	*e820_cur;

	status = EFI_SUCCESS;

	mmap = NULL;
	mmap_size = 0;

retry:

	status = bs->get_memory_map(
		&mmap_size,
		mmap,
		map_key,
		&desc_size,
		&desc_ver);

	if (status == EFI_BUFFER_TOO_SMALL) {
		mmap = malloc(mmap_size);
		goto retry;
	}

	if (EFI_ERROR(status)) /* Failed to get UEFI memory map */
		return status;

	/* Allocate an E820 memory map with the same number of entries */
	e820_cur = handover->mmap;
	e820_entries = mmap_size / desc_size;

	/* Make sure this firmware's memory map is compatible with the kernel */
	if (e820_entries > GRR_MMAP_MAX_ENTRIES) {
		free(mmap);
		return EFI_UNSUPPORTED;
	}

	handover->mmap_entries = e820_entries;

	/* Convert UEFI memory map to E820 */
	for (mmap_ent = mmap; (void *) mmap_ent < mmap + mmap_size;
				mmap_ent = (void *) mmap_ent + desc_size) {
		e820_cur->addr = mmap_ent->start;
		#define PAGE_SIZE 4096 /* TODO: make sure this is always true */
		e820_cur->size = mmap_ent->number_of_pages * PAGE_SIZE;

		/* Make sure we won't be trashed */
		if (e820_cur->addr == (efi_u64) self_loaded_image->image_base)
			mmap_ent->type = efi_runtime_services_code;

		switch (mmap_ent->type) {
		case efi_conventional_memory:
		case efi_loader_code:
		case efi_loader_data:
		case efi_boot_services_code:
		case efi_boot_services_data:
			e820_cur->type = GRR_E820_USABLE;
			break;
		case efi_acpi_reclaim_memory:
			e820_cur->type = GRR_E820_ACPI_RECLAIM;
			break;
		case efi_acpi_memory_nvs:
			e820_cur->type = GRR_E820_APCI_NVS;
			break;
		case efi_unusable_memory:
			e820_cur->type = GRR_E820_UNUSABLE;
			break;
		/* Default to reserved */
		default:
			e820_cur->type = GRR_E820_RESERVED;
			break;
		}
		++e820_cur;
	}

	/* NOTE: the UEFI mmap cannot be freed otherwise UEFI shits itself */
	return status;
}

#define LINUX_PATH	L"\\efi\\grr\\vmlinuz-4.19.0-10-amd64"
#define INITRD_PATH	L"\\efi\\grr\\initrd.img-4.19.0-10-amd64"
#define CMDLINE		"root=/dev/sda2 rw"

void
efiapi
efi_main(efi_handle image_handle, efi_system_table *system_table)
{
	efi_status status;
	grr_handover	*handover;
	efi_size	map_key;

	/* Setup efiutil */
	init_util(image_handle, system_table);

	print(L"GRR efi stage entry\n");
	handover = malloc(sizeof(grr_handover));

	/* Load kernel and initrd */
	status = load_file(LINUX_PATH, &handover->linux_size, &handover->linux);
	if (EFI_ERROR(status))
		abort(L"Failed to load kernel!\n", status);
	status = load_file(INITRD_PATH,
			&handover->initrd_size, &handover->initrd);
	if (EFI_ERROR(status))
		abort(L"Failed to load initrd!\n", status);
	/* Pointer to the command line */
	handover->cmdline = CMDLINE;

	/* Fill handover block */
	status = fill_video(handover);
	if (EFI_ERROR(status))
		print(L"WARN: No suitable framebuffer found!\n");

	/* Must do this before getting the
		memory map as this might allocate memory */
	fini_util();

	status = fill_mmap(handover, &map_key);
	if (EFI_ERROR(status))
		abort(L"Failed to get memory map!\n", status);

	status = bs->exit_boot_services(image_handle, map_key);
	if (EFI_ERROR(status))
		abort(L"Failed to exit boot services\n", status);

	/* Call into hv stage */
	grr_hv_entry(handover);
}
