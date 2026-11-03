__bootloader_start = 0x00000000;
__bootloader_end = 0x00008000;
__application_slot_start = 0x00008000;
__application_slot_end = 0x00048000;
__staging_slot_start = 0x00048000;
__staging_slot_end = 0x00088000;
__status_journal_start = 0x00088000;
__status_journal_end = 0x0008C000;
__ram_start = 0x20000000;
__ram_end = 0x20040000;

ASSERT(__staging_slot_end - __staging_slot_start == 256K, "the staging slot is not 256 KB")
