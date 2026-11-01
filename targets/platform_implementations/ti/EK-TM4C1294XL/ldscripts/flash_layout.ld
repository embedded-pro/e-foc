/* Flash layout of documentation/design/bootloader.md, Part A */
__bootloader_start = 0x00000000;
__bootloader_end = 0x00008000;
__application_slot_start = 0x00008000;
__application_slot_end = 0x00084000;
__staging_slot_start = 0x00084000;
__staging_slot_end = 0x000FC000;
__status_journal_start = 0x000FC000;
__status_journal_end = 0x00100000;
__ram_start = 0x20000000;
__ram_end = 0x20040000;
