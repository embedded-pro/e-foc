__bootloader_start = 0x00000000;
__bootloader_end = 0x00008000;
__application_slot_start = 0x00008000;
__application_slot_end = 0x00058000;
__staging_slot_start = 0x00058000;
__staging_slot_end = 0x000A8000;
__status_journal_start = 0x000A8000;
__status_journal_end = 0x000AC000;
__ram_start = 0x20000000;
__ram_end = 0x20040000;

ASSERT(__application_slot_end - __application_slot_start == 320K, "the application slot is not 320 KB")
ASSERT(__staging_slot_end - __staging_slot_start == 320K, "the staging slot is not 320 KB")
