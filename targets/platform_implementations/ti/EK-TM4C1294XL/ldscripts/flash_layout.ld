/* The 1 MB flash is two independent halves: banks 0 and 1 below 0x80000, banks 2 and 3 above it. A sector
   erase stalls every read of its own half until it completes, instruction fetches and the watchdog's
   early-warning handler included, while the other half reads normally (TM4C1294 datasheet 8.2.3.3, mirror
   mode). The application executes from the lower half and erases the staging slot and the journal while it
   runs, so both lie entirely in the upper half, where their erases cannot block the core or the watchdog. */
__flash_upper_half_start = 0x00080000;
__flash_end = 0x00100000;

__bootloader_start = 0x00000000;
__bootloader_end = 0x00008000;
__application_slot_start = 0x00008000;
__application_slot_end = 0x00058000;
__staging_slot_start = 0x00080000;
__staging_slot_end = 0x000D0000;
__status_journal_start = 0x000D0000;
__status_journal_end = 0x000D4000;
__ram_start = 0x20000000;
__ram_end = 0x20040000;

ASSERT(__application_slot_end - __application_slot_start == 320K, "the application slot is not 320 KB")
ASSERT(__staging_slot_end - __staging_slot_start == 320K, "the staging slot is not 320 KB")
ASSERT(__application_slot_end <= __flash_upper_half_start, "the application slot reaches into the upper flash half")
ASSERT(__staging_slot_start >= __flash_upper_half_start, "the staging slot is not entirely in the upper flash half, so its erases would stall the application and its watchdog")
ASSERT(__status_journal_start >= __flash_upper_half_start, "the status journal is not in the upper flash half, so its erases would stall the application and its watchdog")
ASSERT(__status_journal_end <= __flash_end, "the status journal lies beyond the flash")
