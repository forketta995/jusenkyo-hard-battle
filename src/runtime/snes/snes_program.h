#pragma once

typedef enum {
    BOOT_CHECKPOINT,
    BOOT_UNKNOWN_BLOCK,
    BOOT_UNMAPPED_ACCESS,
    BOOT_AUDIO_FAULT,
    BOOT_STEP_LIMIT,
    BOOT_FRAME,
} BootResult;
