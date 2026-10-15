//-----------------------------------------------------------------------------
// Project:      CETI Tag Electronics
// Copyright:    Harvard University Wood Lab
// Contributors: Michael Salino-Hugg
//
// Nonvolatile memory settings and programming, following
// whale-tag-stm32/src/battery/bms_ctl.c
//-----------------------------------------------------------------------------
#include "bms_ctl.h"
#include "max17320.h"

#include <stdio.h>

typedef struct {
    char *name;
    uint16_t addr;
    uint16_t value;
} NvExpected;

#define BMS_VERSION_1_0 (0)
#define BMS_VERSION_1_1 (1)

#define BMS_VERSION (BMS_VERSION_1_1)

#define BMS_CELL_EFEST_18650 (0)
#define BMS_CELL_605060_2C (1)

#define BMS_CELLS BMS_CELL_605060_2C

#if BMS_CELLS == BMS_CELL_605060_2C
#define BMS_CELL_CAPACITY_mAh 2050
#elif BMS_CELLS == BMS_CELL_EFEST_18650
#define BMS_CELL_CAPACITY_mAh 4000
#endif // BMS_CELLS

// refuse to program once this few nonvolatile updates remain, so that a bad
// write can still be corrected
#define BMS_MIN_REMAINING_NV_WRITES 3

const NvExpected g_nv_expected[] = {
    {.name = "NRSENSE", .addr = 0x1cf, .value = 0x03e8},
    {.name = "NDESIGNCAP", .addr = 0x1b3, .value = BMS_CELL_CAPACITY_mAh * 10 / 5},
#if BMS_VERSION == BMS_VERSION_1_1
    {.name = "NPACKCFG", .addr = 0x1b5, .value = 0xc204},
#elif BMS_VERSION == BMS_VERSION_1_0
    {.name = "NPACKCFG", .addr = 0x1b5, .value = 0xc208},
#endif // BMS_VERSION
    {.name = "NNVCFG0", .addr = 0x1b8, .value = 0x0830},
    {.name = "NNVCFG1", .addr = 0x1b9, .value = 0x2100},
    {.name = "NNVCFG2", .addr = 0x1ba, .value = 0x822d},
    {.name = "NUVPRTTH", .addr = 0x1d0, .value = 0xa002},
    {.name = "NTPRTTH1", .addr = 0x1d1, .value = 0x280a},
    {.name = "NIPRTTH1", .addr = 0x1d3, .value = 0x32ce},
    {.name = "NBALTH", .addr = 0x1d4, .value = 0x0ca0},
    {.name = "NPROTMISCTH", .addr = 0x1d6, .value = 0x0813},
    {.name = "NPROTCFG", .addr = 0x1d7, .value = 0x0c08},
    {.name = "NJEITAV", .addr = 0x1d9, .value = 0xec00},
    {.name = "NOVPRTTH", .addr = 0x1da, .value = 0xb3a0},
    {.name = "NDELAYCFG", .addr = 0x1dc, .value = 0x0035},
    {.name = "NODSCCFG", .addr = 0x1de, .value = 0x4058},
    {.name = "NCONFIG", .addr = 0x1b0, .value = 0x0290},
    {.name = "NTHERMCFG", .addr = 0x1ca, .value = 0x71be},
    {.name = "NVEMPTY", .addr = 0x19e, .value = 0x9659},
    {.name = "NFULLSOCTHR", .addr = 0x1c6, .value = 0x5005},
    {.name = NULL},
};

// Compares the shadow RAM against g_nv_expected. Returns 1 if every value
// matches, 0 if any differ, and -1 if a register could not be read.
static int nv_compare(int verbose) {
    int incorrect = 0;
    for (int i = 0; g_nv_expected[i].name != NULL; i++) {
        uint16_t actual;

        // hardware access register
        if (max17320_read(g_nv_expected[i].addr, &actual) < 0) {
            printf("Err: could not read %s\n", g_nv_expected[i].name);
            return -1;
        }

        // assertions
        if (actual != g_nv_expected[i].value) {
            if (verbose) {
                printf("%-12s: 0x%04x != 0x%04x !!!!\n", g_nv_expected[i].name, actual, g_nv_expected[i].value);
            }
            incorrect++;
        } else if (verbose) {
            printf("%-12s: 0x%04x  OK!\n", g_nv_expected[i].name, actual);
        }
    }

    if (incorrect != 0) {
        if (verbose) {
            printf("%d values did not match expected value\n", incorrect);
        }
        return 0;
    }
    return 1;
}

/**
 * @brief Read BMS nonvolatile memory and verifies values are the expected value.
 *
 * @return int 1 if match, 0 if not, -1 on a read error
 */
int bms_ctl_verify(void) {
    printf("Nonvolatile RAM Settings:\n");
    return nv_compare(1);
}

/**
 * @brief Writes g_nv_expected to shadow RAM and commits it to nonvolatile
 * memory. Skipped when the values are already stored, since the IC only
 * allows a limited number of nonvolatile updates.
 *
 * @return int 0 on success, -1 on failure
 */
int bms_ctl_program_nonvolatile_memory(void) {
    int verified = bms_ctl_verify();
    if (verified < 0) {
        return -1;
    }
    if (verified == 1) {
        printf("Nonvolatile memory already holds the expected values, not rewriting\n");
        return 0;
    }

    if (max17320_get_remaining_writes(&dev) < 0) {
        printf("Err: could not read the remaining nonvolatile updates\n");
        return -1;
    }
    printf("Remaining nonvolatile updates: %u\n", dev.remaining_writes);
    if (dev.remaining_writes < BMS_MIN_REMAINING_NV_WRITES) {
        printf("Err: fewer than %d nonvolatile updates remain, not rewriting\n", BMS_MIN_REMAINING_NV_WRITES);
        return -1;
    }

    if (max17320_clear_write_protection(&dev) < 0) {
        printf("Err: could not clear write protection\n");
        return -1;
    }

    // write shadowram
    for (int i = 0; g_nv_expected[i].name != NULL; i++) {
        // hardware access register
        if (max17320_write(&dev, g_nv_expected[i].addr, g_nv_expected[i].value) < 0) {
            printf("Err: could not write %s\n", g_nv_expected[i].name);
            return -1;
        }
    }

    // only spend a nonvolatile update on values that made it into shadow RAM
    if (nv_compare(0) != 1) {
        printf("Err: shadow RAM did not take the expected values, not committing\n");
        return -1;
    }

    printf("Committing shadow RAM to nonvolatile memory...\n");
    if (max17320_swap_shadow_ram(&dev) < 0) {
        printf("Err: block copy to nonvolatile memory failed\n");
        return -1;
    }

    // the reset recalled shadow RAM from nonvolatile memory, so this reads
    // back what was stored
    if (bms_ctl_verify() != 1) {
        printf("Err: nonvolatile memory does not hold the expected values after programming\n");
        return -1;
    }

    if (max17320_lock_write_protection(&dev) < 0) {
        printf("Err: could not re-enable write protection\n");
        return -1;
    }
    printf("Nonvolatile memory programmed\n");
    return 0;
}
