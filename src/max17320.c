//-----------------------------------------------------------------------------
// Project:      CETI Tag Electronics
// Version:      Refer to _versioning.h
// Copyright:    Cummings Electronics Labs, Harvard University Wood Lab, MIT CSAIL
// Contributors: Saksham Ahuja, Matt Cummings, Shanaya Barretto, Michael Salino-Hugg
//-----------------------------------------------------------------------------

#include "max17320.h"
#include <pigpio.h>
#include <unistd.h>

static inline max17320_Reg_Status __statusRegister_from_raw(uint16_t raw) {
    return (max17320_Reg_Status) {
        .power_on_reset = _RSHIFT(raw, 1, 1),
        .min_current_alert = _RSHIFT(raw, 2, 1),
	    .max_current_alert = _RSHIFT(raw, 6, 1),
	    .state_of_charge_change_alert = _RSHIFT(raw, 7, 1),
	    .min_voltage_alert = _RSHIFT(raw, 8, 1),
	    .min_temp_alert = _RSHIFT(raw, 9, 1),
	    .min_state_of_charge_alert = _RSHIFT(raw, 10, 1),
	    .max_voltage_alert = _RSHIFT(raw, 12, 1),
	    .max_temp_alert = _RSHIFT(raw, 13, 1),
	    .max_state_of_charge_alert = _RSHIFT(raw, 14, 1),
	    .protection_alert = _RSHIFT(raw, 15, 1),
    };
}

static inline max17320_Reg_ProtStatus __protStatusRegister_from_raw(uint16_t raw) {
    return (max17320_Reg_ProtStatus) {
        .ship = _RSHIFT(raw, 0, 1),
        .perm_fail = _RSHIFT(raw, 6, 1),
        .die_overtemperature = _RSHIFT(raw, 5, 1),
        .res_d_fault = _RSHIFT(raw, 1, 1),
        .overdischarge_current = _RSHIFT(raw, 2, 1),
        .undervoltage = _RSHIFT(raw, 3, 1),
        .too_hot_discharge = _RSHIFT(raw, 4, 1),
        .overcharge_current = _RSHIFT(raw, 10, 1),
        .overvoltage = _RSHIFT(raw, 11, 1),
        .too_hot_charge = _RSHIFT(raw, 14, 1),
        .too_cold_charge = _RSHIFT(raw, 12, 1),
        .capacity_overflow = _RSHIFT(raw, 9, 1),
        .full = _RSHIFT(raw, 13, 1),
        .watchdog_timer = _RSHIFT(raw, 15, 1),
        .imbalance = _RSHIFT(raw, 7, 1),
        .prequal_timeout = _RSHIFT(raw, 8, 1),
    };
}

static inline max17320_Reg_ProtAlrt __protAlrtRegister_from_raw(uint16_t raw) {
    return (max17320_Reg_ProtAlrt) {
        .leak_detection = _RSHIFT(raw, 0, 1),
        .perm_fail = _RSHIFT(raw, 6, 1),
        .die_overtemperature = _RSHIFT(raw, 5, 1),
        .res_d_fault = _RSHIFT(raw, 1, 1),
        .overdischarge_current = _RSHIFT(raw, 2, 1),
        .undervoltage = _RSHIFT(raw, 3, 1),
        .too_hot_discharge = _RSHIFT(raw, 4, 1),
        .overcharge_current = _RSHIFT(raw, 10, 1),
        .overvoltage = _RSHIFT(raw, 11, 1),
        .too_hot_charge = _RSHIFT(raw, 14, 1),
        .too_cold_charge = _RSHIFT(raw, 12, 1),
        .capacity_overflow = _RSHIFT(raw, 9, 1),
        .full = _RSHIFT(raw, 13, 1),
        .watchdog_timer = _RSHIFT(raw, 15, 1),
        .imbalance = _RSHIFT(raw, 7, 1),
        .prequal_timeout = _RSHIFT(raw, 8, 1),
    };
}

int max17320_write(MAX17320_HandleTypeDef *dev, uint16_t memory, uint16_t data) {
    int ret = 0;
    uint16_t addr = MAX17320_ADDR;
    if (memory > 0xFF) {
        memory = memory & 0xFF;
        addr = MAX17320_ADDR_SEC;
    }
    int fd=i2cOpen(1, addr, 0);
    if (fd < 0) {
        // CETI_ERR("Failed to connect to the MAX17320 battery gauge");
        return -1;
    }
    ret = i2cWriteWordData(fd, memory, data);
    i2cClose(fd);
    // TODO: Add error checking but note that there are some exceptions (clearing write)
    return ret;
}

int max17320_read(uint16_t memory, uint16_t *storage) {
    int ret = 0;
    uint16_t addr = MAX17320_ADDR;
    if (memory > 0xFF) {
        memory = memory & 0xFF;
        addr = MAX17320_ADDR_SEC;
    }
    int fd=i2cOpen(1, addr, 0);
    if (fd < 0) {
        // CETI_ERR("Failed to connect to the MAX17320 battery gauge");
        return -1;
    }
    int word = i2cReadWordData(fd, memory);
    i2cClose(fd);
    if (word < 0) {
        // pigpio reports bus errors as a negative status, which would otherwise
        // be truncated into a plausible-looking register value
        return -1;
    }
    *storage = (uint16_t)word;
    return ret;
}

int max17320_clear_write_protection(MAX17320_HandleTypeDef *dev) {
    uint16_t read = 0;
    int ret = 0;

    uint8_t counter = 2;
    while (counter > 0)
    {
        ret |= max17320_write(dev, MAX17320_REG_COMM_STAT, CLEAR_WRITE_PROT);
        usleep(TRECALL_US);
        counter--;
    }
    ret |= max17320_read(MAX17320_REG_COMM_STAT, &read);
    if (read == CLEARED_WRITE_PROT || read == CLEAR_WRITE_PROT)
    {
        return ret;
    }
    // CETI_ERR("MAX17320 Clearing write protection failed, CommStat: 0x%.4x", read);
    ret = -1;
    return ret;
}

int max17320_lock_write_protection(MAX17320_HandleTypeDef *dev) {
    uint16_t read = 0;
    uint8_t counter = 2;
    int ret = 0;
    while (counter > 0)
    {
        ret |= max17320_write(dev, MAX17320_REG_COMM_STAT, LOCKED_WRITE_PROT);
        usleep(TRECALL_US);
        counter--;
    }
    ret |= max17320_read(MAX17320_REG_COMM_STAT, &read);
    if (read != LOCKED_WRITE_PROT)
    {
        // CETI_ERR("MAX17320 Locking write protection failed, CommStat: 0x%.4x", read);
        ret = -1;
    }
    return ret;
}

int max17320_get_status(MAX17320_HandleTypeDef *dev) {
    uint16_t read = 0;
    int ret = max17320_read(MAX17320_REG_STATUS, &read);
    if (ret >= 0) {
        dev->status = __statusRegister_from_raw(read);
        // CETI_DEBUG("MAX17320 Status: 0x%.4x", read);
    }
    return ret;
}

int max17320_get_prot_status(MAX17320_HandleTypeDef *dev) {
    uint16_t read = 0;
    int ret = max17320_read(MAX17320_REG_PROTSTATUS, &read);
    if (ret >= 0) {
        dev->prot_status = __protStatusRegister_from_raw(read);
        // CETI_DEBUG("MAX17320 ProtStatus: 0x%.4x", read);
    }
    return ret;
}

int max17320_get_prot_alrt(MAX17320_HandleTypeDef *dev) {
    uint16_t read = 0;
    int ret = max17320_read(MAX17320_REG_PROTALRT, &read);
    if (ret >= 0) {
        dev->prot_alert = __protAlrtRegister_from_raw(read);
        // CETI_DEBUG("MAX17320 ProtAlrt: 0x%.4x", read);
    }
    return ret;
}

int max17320_get_remaining_capacity(MAX17320_HandleTypeDef* dev) {
    uint16_t read = 0;
    int ret = max17320_read(MAX17320_REG_REP_CAPACITY, &read);
    if (ret >= 0) {
        dev->remaining_capacity = read * (CAPACITY_LSB / R_SENSE_VAL);
        // CETI_DEBUG("MAX17320 Remaining Capacity: %.2f mAh", dev->remaining_capacity);
    }
    return ret;
}

int max17320_get_state_of_charge(MAX17320_HandleTypeDef *dev) {
    uint16_t read = 0;
    int ret = max17320_read(MAX17320_REG_REP_SOC, &read);
    if (ret >= 0) {
        dev->state_of_charge = read * PERCENTAGE_LSB;
        // CETI_DEBUG("MAX17320 State of Charge: %.2f %%", dev->state_of_charge);
    }
    return ret;
}

int max17320_get_voltages(MAX17320_HandleTypeDef *dev) {
    uint16_t read = 0;
    // Cell 1 Voltage
    int ret = max17320_read(MAX17320_REG_CELL1_VOLTAGE, &read);
    if (ret >= 0) {
        dev->cell_1_voltage = read * CELL_VOLTAGE_LSB;
        // CETI_DEBUG("MAX17320 Cell 1 Voltage: %.2f V", dev->cell_1_voltage);
    }

    // Cell 2 Voltage
    ret |= max17320_read(MAX17320_REG_CELL2_VOLTAGE, &read);
    if (ret >= 0) {
        dev->cell_2_voltage = read * CELL_VOLTAGE_LSB;
        // CETI_DEBUG("MAX17320 Cell 2 Voltage: %.2f V", dev->cell_2_voltage);
    }

    // Total Battery Voltage
    ret |= max17320_read(MAX17320_REG_TOTAL_BAT_VOLTAGE, &read);
    if (ret >= 0) {
        dev->total_battery_voltage = read * PACK_VOLTAGE_LSB;
        // CETI_DEBUG("MAX17320 Total Battery Voltage: %.2f V", dev->total_battery_voltage);
    }
    
    // Pack Side Voltage
    ret |= max17320_read(MAX17320_REG_PACK_SIDE_VOLTAGE, &read);
    if (ret >= 0) {
        dev->pack_side_voltage = read * PACK_VOLTAGE_LSB;
        // CETI_DEBUG("MAX17320 Pack Side Voltage: %.2f V", dev->pack_side_voltage);
    }
    
    return ret;
}

int max17320_get_temperature(MAX17320_HandleTypeDef *dev) {
    uint16_t read = 0;
    int ret = max17320_read(MAX17320_REG_TEMPERATURE, &read);
    if (ret >= 0) {
        dev->temperature = (int16_t)read * TEMPERATURE_LSB;
        // CETI_DEBUG("MAX17320 Thermistor Temperature: %.2f °C", dev->temperature);
    }
    ret |= max17320_read(MAX17320_REG_DIETEMP, &read);
    if (ret >= 0) {
        dev->die_temperature = (int16_t)read * TEMPERATURE_LSB;
        // CETI_DEBUG("MAX17320 Die Temperature: %.2f °C", dev->die_temperature);
    }
    return ret;
}

int max17320_get_battery_current(MAX17320_HandleTypeDef *dev) {
    uint16_t read = 0;
    int ret = max17320_read(MAX17320_REG_BATT_CURRENT, &read);
    if (ret >= 0) {
        // CURRENT_LSB is in uV, R_SENSE_VAL in Ω, so the quotient is uA
        dev->battery_current = ((int16_t)read * (CURRENT_LSB/R_SENSE_VAL)) / 1000000.0;
        // CETI_DEBUG("MAX17320 Battery Current: %.3f A", dev->battery_current);
    }
    return ret;
}

int max17320_get_average_battery_current(MAX17320_HandleTypeDef *dev) {
    uint16_t read = 0;
    int ret = max17320_read(MAX17320_REG_AVG_BATT_CURRENT, &read);
    if (ret >= 0) {
        dev->average_current = ((int16_t)read * (CURRENT_LSB/R_SENSE_VAL)) / 1000000.0;
        // CETI_DEBUG("MAX17320 Avg Battery Current: %.3f A", dev->average_current);
    }
    return ret;
}

int max17320_get_time_to_empty(MAX17320_HandleTypeDef *dev) {
    uint16_t read = 0;
    int ret = max17320_read(MAX17320_REG_TIME_TO_EMPTY, &read);
    if (ret >= 0) {
        dev->time_to_empty = read * (TIME_LSB / SECOND_TO_HOUR);
        // CETI_DEBUG("MAX17320 Time to Empty: %.2f hrs", dev->time_to_empty);
    }
    return ret;
}
int max17320_get_time_to_full(MAX17320_HandleTypeDef *dev) {
    uint16_t read = 0;
    int ret = max17320_read(MAX17320_REG_TIME_TO_FULL, &read);
    if (ret >= 0) {
        dev->time_to_full = read * (TIME_LSB / SECOND_TO_HOUR);
        // CETI_DEBUG("MAX17320 Time to Full: %.2f hrs", dev->time_to_full);
    }
    return ret;
}

// Resets the recorded current peaks so that tracking restarts from the present
// reading. MaxMinCurr sits in the write-protected range, so protection has to
// be cleared first; it is left cleared, as elsewhere in this driver, because
// re-locking writes 0x00F9 to CommStat and would re-enable both FETs.
int max17320_clear_max_min_current(MAX17320_HandleTypeDef *dev) {
    int ret = max17320_clear_write_protection(dev);
    if (ret < 0) {
        // CETI_ERR("MAX17320 could not clear write protection to reset MaxMinCurr");
        return ret;
    }
    ret = max17320_write(dev, MAX17320_REG_MAXMINCURR, MAXMINCURR_RESET);
    if (ret < 0) {
        // CETI_ERR("MAX17320 could not reset MaxMinCurr");
        return ret;
    }
    // no read-back check: the gauge overwrites both peaks with the present
    // current on its next conversion, so a mismatch here is not an error
    // CETI_LOG("MAX17320 MaxMinCurr reset");
    return 0;
}

int max17320_gauge_reset(MAX17320_HandleTypeDef *dev) {
    uint16_t read = 0;
    int ret = max17320_clear_write_protection(dev);
    // Reset firmware
    ret |= max17320_write(dev, MAX17320_REG_CONFIG2, MAX17320_RESET_FW);
    ret |= max17320_read(MAX17320_REG_CONFIG2, &read);
    // Wait for POR_CMD bit to clear
    int attempts = MAX17320_TIMEOUT;
    while ((read & 0x8000) == 0x8000) {
        if (attempts-- <= 0) {
            // CETI_ERR("MAX17320 POR_CMD never cleared after reset");
            return -1;
        }
        usleep(TRECALL_US);
        ret |= max17320_read(MAX17320_REG_CONFIG2, &read);
        // CETI_DEBUG("MAX17320 waiting for bit to clear post reset");
    }
    return ret;
}

int max17320_reset(MAX17320_HandleTypeDef *dev) {
    // Performs full reset
    int ret = max17320_clear_write_protection(dev);
    ret |= max17320_write(dev, MAX17320_REG_COMMAND, MAX17320_RESET);
    usleep(10000);
    ret |= max17320_gauge_reset(dev);
    return ret;
}

// Commits the whole shadow RAM (180h-1FFh) to nonvolatile memory, then resets
// the IC so that the shadow RAM is recalled from what was actually stored.
// Each call uses up one of the limited nonvolatile updates.
int max17320_swap_shadow_ram(MAX17320_HandleTypeDef *dev) {
    // Clear CommStat.NVError bit
    int ret = max17320_write(dev, MAX17320_REG_COMM_STAT, CLEAR_WRITE_PROT);
    if (ret < 0) {
        return ret;
    }

    // Initiate a block copy
    ret = max17320_write(dev, MAX17320_REG_COMMAND, INITIATE_BLOCK_COPY);
    if (ret < 0) {
        return ret;
    }
    usleep(TBLOCK_US);

    // wait for block copy to complete
    uint16_t read = 0;
    int attempts = MAX17320_TIMEOUT;
    do {
        if (attempts-- <= 0) {
            // CETI_ERR("MAX17320 block copy never completed");
            return -1;
        }
        if (max17320_read(MAX17320_REG_COMM_STAT, &read) < 0) {
            return -1;
        }
        usleep(TRECALL_US);
    } while (read & COMMSTAT_NVBUSY);

    if (read & COMMSTAT_NVERROR) {
        // CETI_ERR("MAX17320 block copy failed, CommStat: 0x%.4x", read);
        return -1;
    }

    return max17320_reset(dev);
}

int max17320_get_remaining_writes(MAX17320_HandleTypeDef *dev) {
    uint16_t read = 0;
    // Clear write protection
    int ret = max17320_clear_write_protection(dev);
    if (ret < 0)
    {
        return ret;
    }
    
    // Write to command register
    ret = max17320_write(dev, MAX17320_REG_COMMAND, DETERMINE_REMAINING_UPDATES);
    usleep(TRECALL_US);

    // Read from register that holds remaining writes
    ret |= max17320_read(MAX17320_REG_REMAINING_WRITES, &read);
    // CETI_DEBUG("MAX17320 Remaining Writes Register Read: 0x%.4x", read);

    // Decode remaining writes
    uint8_t first_byte = (read>>8) & 0xff;
    uint8_t last_byte = read & 0xff;
    uint8_t decoded = first_byte | last_byte;
    uint8_t count = 0;
    while (decoded > 0)
    {
        if (decoded & 1)
        {
            count++;
        }
        decoded = decoded >> 1;
    }
    dev->remaining_writes = (8-count);
    // CETI_LOG("MAX17320 Remaining Writes: %u", dev->remaining_writes);
    // Commented out for now, to not affect previous settings for FETS when unlocking
    // ret |= max17320_lock_write_protection(dev);
    return ret;
}

// CHARGE_ON/DISCHARGE_ON are AND masks and CHARGE_OFF/DISCHARGE_OFF are OR masks,
// so CommStat has to be read back and modified. Writing a mask directly would
// clobber the other FET and re-assert the write-protection bits (0x00F9).
static inline int max17320_set_comm_stat(MAX17320_HandleTypeDef *dev, uint16_t and_mask, uint16_t or_mask) {
    uint16_t read = 0;
    int ret = max17320_read(MAX17320_REG_COMM_STAT, &read);
    if (ret < 0) {
        return ret;
    }
    return max17320_write(dev, MAX17320_REG_COMM_STAT, (read & and_mask) | or_mask);
}

int max17320_enable_charging(MAX17320_HandleTypeDef *dev) {
    int ret = max17320_set_comm_stat(dev, CHARGE_ON, 0x0000);
    if (ret >= 0) {
        // CETI_LOG("MAX17320 Charge FET Enabled");
    }
    else {
        // CETI_ERR("MAX17320 Charge FET could not be enabled");
    }
    return ret;
}

int max17320_enable_discharging(MAX17320_HandleTypeDef *dev) {
    int ret = max17320_set_comm_stat(dev, DISCHARGE_ON, 0x0000);
    if (ret >= 0) {
        // CETI_LOG("MAX17320 Discharge FET Enabled");
    }
    else {
        // CETI_ERR("MAX17320 Discharge FET could not be enabled");
    }
    return ret;
}

int max17320_disable_charging(MAX17320_HandleTypeDef *dev) {
    int ret = max17320_set_comm_stat(dev, 0xFFFF, CHARGE_OFF);
    if (ret >= 0) {
        // CETI_LOG("MAX17320 Charge FET Disabled");
    }
    else {
        // CETI_ERR("MAX17320 Charge FET could not be disabled");
    }
    return ret;
}

int max17320_disable_discharging(MAX17320_HandleTypeDef *dev) {
    int ret = max17320_set_comm_stat(dev, 0xFFFF, DISCHARGE_OFF);
    if (ret >= 0) {
        // CETI_LOG("MAX17320 Discharge FET Disabled");
    }
    else {
        // CETI_ERR("MAX17320 Discharge FET could not be disabled");
    }
    return ret;
}

// Opens both FETs, cutting current through the pack in both directions. Both
// bits are set in a single CommStat write: if the host runs off the pack,
// opening the discharge FET first would cut its power before the charge FET
// could be opened. Write protection is cleared first and left cleared, since
// clearing it after would rewrite CommStat and close both FETs again.
int max17320_shutdown(MAX17320_HandleTypeDef *dev) {
    int ret = max17320_clear_write_protection(dev);
    if (ret < 0) {
        // CETI_ERR("MAX17320 could not clear write protection to open the FETs");
        return ret;
    }
    ret = max17320_set_comm_stat(dev, 0xFFFF, CHARGE_OFF | DISCHARGE_OFF);
    if (ret >= 0) {
        // CETI_LOG("MAX17320 Charge and Discharge FETs Disabled");
    }
    else {
        // CETI_ERR("MAX17320 FETs could not be disabled");
    }
    return ret;
}
