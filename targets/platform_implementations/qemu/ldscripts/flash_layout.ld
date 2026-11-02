/* Emulated layout of documentation/design/bootloader.md, Part A: the code RAM holds the bootloader and the application
   slot; staging and journal are semihosting files, so their symbols give only their sizes */
__bootloader_start = 0x00000000;
__bootloader_end = 0x00008000;
__application_slot_start = 0x00008000;
__application_slot_end = 0x00048000;
__staging_slot_start = 0;
__staging_slot_end = 256K;
__status_journal_start = 0;
__status_journal_end = 16K;
__ram_start = 0x20000000;
__ram_end = 0x20400000;
