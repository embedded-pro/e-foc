__bootloader_start = 0x00000000;
__bootloader_end = 0x00008000;
__application_slot_start = 0x00008000;
__application_slot_end = 0x00058000;
__staging_slot_start = 0;
__staging_slot_end = 320K;
__status_journal_start = 0;
__status_journal_end = 32K;
__ram_start = 0x20000000;
__ram_end = 0x20400000;

ASSERT(__status_journal_end - __status_journal_start >= 2 * 16K, "the status journal has fewer than two 16 KB erase units, so the bootloader stops at start-up")
