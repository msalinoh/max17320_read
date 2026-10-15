#include <pigpio.h>
#include "max17320.h"
#include "bms_ctl.h"
#include <stdint.h>
#include <string.h>
#include <strings.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <time.h>

#define _RSHIFT(val, s, w) (((val) >> (s)) & ((1 << (w)) - 1)) 


typedef struct registers_desc_e {
    char * name;
    uint16_t address;
    void (*from_raw)(struct registers_desc_e *desc, uint16_t *dst);
    void (*value_2_str)(char * string, size_t size, uint16_t value);
} RegisterDescription;

void simple_get(struct registers_desc_e *desc, uint16_t *x){max17320_read(desc->address, x);}
void test_get(struct registers_desc_e *desc, uint16_t * x) {*x = desc->address;}
void test_parse(char * string, size_t size, uint16_t value) {snprintf(string, size, "\"0x%04x\"", value);}

// renders the set flags as a JSON string ("A | B"), or nil if none are set
static void flags_to_json(char * string, size_t size, char ** flags, int n) {
    if (n == 0) {
        snprintf(string, size, "nil");
        return;
    }
    int written = snprintf(string, size, "\"");
    if (written < 0) {
        return;
    }
    size_t len = (size_t)written;
    for (int j = 0; j < n; j++) {
        if (len >= size) {
            return;
        }
        written = snprintf(string + len, size - len, "%s%s", (j == 0) ? "" : " | ", flags[j]);
        if (written < 0) {
            return;
        }
        len += (size_t)written;
    }
    if (len < size) {
        snprintf(string + len, size - len, "\"");
    }
}

double get_rsense_mOhm(void) {
    uint16_t resense_raw = 0;
    // nRSense LSB is 10 uOhm
    if (max17320_read(0x01CF, &resense_raw) < 0 || resense_raw == 0) {
        // fall back to the board's design value rather than dividing by zero
        return R_SENSE_VAL*1000.0;
    }
    return 0.01*resense_raw;
}

void nAgeFcCfg_to_str(char * string, size_t size, uint16_t value) {snprintf(string, size, "{\"DeadTargetRatio\": \"\", \"CycleStart\": \"\"}");}
void nPackCfg_to_str(char * string, size_t size, uint16_t raw) {
    snprintf(string, size, "{\"NCELLS\": %d}", _RSHIFT(raw, 0, 2) + 2);
};

void Status_to_str(char * string, size_t size, uint16_t raw) {
    char* flags [16] = {};
    int i = 0;
    if(_RSHIFT(raw, 1, 1)) flags[i++] = "POR";
    if(_RSHIFT(raw, 7, 1)) flags[i++] = "dSOCi";
    if(_RSHIFT(raw, 2, 1)) flags[i++] = "Imn";
    if(_RSHIFT(raw, 6, 1)) flags[i++] = "Imx";
    if(_RSHIFT(raw, 8, 1)) flags[i++] = "Vmn";
    if(_RSHIFT(raw, 12, 1)) flags[i++] = "Vmx";
    if(_RSHIFT(raw, 9, 1)) flags[i++] = "Tmn";
    if(_RSHIFT(raw, 13, 1)) flags[i++] = "Tmx";
    if(_RSHIFT(raw, 10, 1)) flags[i++] = "Smn";
    if(_RSHIFT(raw, 14, 1)) flags[i++] = "Smx";
    if(_RSHIFT(raw, 15, 1)) flags[i++] = "PA";

    flags_to_json(string, size, flags, i);
}
 
void VAlrtTh_to_str(char * string, size_t size, uint16_t raw){
    double vmax = VOLTAGE_ALT_LSB*(raw>>8);
    double vmin = VOLTAGE_ALT_LSB*(raw& 0xff);
    snprintf(string, size, "{\"VMAX\": \"%.2f v\", \"VMIN\": \"%.2f v\"}", vmax, vmin);
}

void TAlrtTh_to_str(char * string, size_t size, uint16_t raw){
    int8_t tmax = (raw >> 8) & 0xFF;
    int8_t tmin = (raw & 0xff);
    snprintf(string, size, "{\"TMAX\": \"%d C\", \"TMIN\": \"%d C\"}", tmax, tmin);
}

void SAlrtTh_to_str(char * string, size_t size, uint16_t raw){
    uint8_t smax = (raw >> 8) & 0xFF;
    uint8_t smin = (raw & 0xff);
    snprintf(string, size, "{\"SMAX\": \"%d%%\", \"SMIN\": \"%d%%\"}", smax, smin);
}

void AtRate_to_str(char * string, size_t size, uint16_t raw){}
void RepCap_to_str(char * string, size_t size, uint16_t raw){}
void RepSOC_to_str(char * string, size_t size, uint16_t raw){}
void Age_to_str(char * string, size_t size, uint16_t raw){}
void MaxMinVolt_to_str(char * string, size_t size, uint16_t raw){
    double vmax = VOLTAGE_ALT_LSB*(raw>>8);
    double vmin = VOLTAGE_ALT_LSB*(raw& 0xff);
    snprintf(string, size, "{\"MaxVCell\": \"%.2f v\", \"MinVCell\": \"%.2f v\"}", vmax, vmin);
}
void MaxMinTemp_to_str(char * string, size_t size, uint16_t raw){
    int8_t tmax = (raw >> 8) & 0xFF;
    int8_t tmin = (raw & 0xff);
    snprintf(string, size, "{\"TMAX\": \"%d C\", \"TMIN\": \"%d C\"}", tmax, tmin);
}
void MaxMinCurr_to_str(char * string, size_t size, uint16_t raw){
    double rsense_mOhm = get_rsense_mOhm();
    // LSB is 0.4 mV across the sense resistor; mV / mOhm == A
    double imax_A = CURRENT_ALT_LSB*((int8_t) ((raw >> 8) & 0xFF))/rsense_mOhm;
    double imin_A = CURRENT_ALT_LSB*((int8_t) (raw & 0xff))/rsense_mOhm;
    snprintf(string, size, "{\"MaxCurrent\": \"%f A\", \"MinCurrent\": \"%f A\"}", imax_A, imin_A);
}

void Config_to_str(char * string, size_t size, uint16_t raw){
// void QResidual_to_str(char * string, size_t size, uint16_t raw){}
// void MixSOC_to_str(char * string, size_t size, uint16_t raw){}
// void AvSOC_to_str(char * string, size_t size, uint16_t raw){}
// void MiscCfg_to_str(char * string, size_t size, uint16_t raw){}
    char* flags [16] = {};
    int i = 0;
    if(_RSHIFT(raw, 14, 1)) flags[i++] = "SS";
    if(_RSHIFT(raw, 13, 1)) flags[i++] = "TS";
    if(_RSHIFT(raw, 12, 1)) flags[i++] = "VS";
    if(_RSHIFT(raw, 11, 1)) flags[i++] = "DisLDO";
    if(_RSHIFT(raw, 10, 1)) flags[i++] = "PBen";
    if(_RSHIFT(raw, 9, 1)) flags[i++] = "DisBlockRead";
    if(_RSHIFT(raw, 7, 1)) flags[i++] = "SHIP";
    if(_RSHIFT(raw, 6, 1)) flags[i++] = "COMMSH";
    if(_RSHIFT(raw, 3, 1)) flags[i++] = "FTHRM";
    if(_RSHIFT(raw, 2, 1)) flags[i++] = "Aen";
    if(_RSHIFT(raw, 0, 1)) flags[i++] = "PAen";

    flags_to_json(string, size, flags, i);
}


void CapacityReg_to_str(char * string, size_t size, uint16_t raw){
    double rsense_mOhm = get_rsense_mOhm();
    // LSB is 5.0 uVh across the sense resistor; uVh / mOhm == mAh
    double charge_uVh = (CAPACITY_LSB*1000.0)*raw;
    snprintf(string, size, "\"%f mAh\"", charge_uVh/rsense_mOhm);
}

void PercentageReg_to_str(char * string, size_t size, uint16_t raw){
    double percent = (double)raw*PERCENTAGE_LSB;
    snprintf(string, size, "\"%.1f%%\"", percent);
}

void TimeReg_to_str(char * string, size_t size, uint16_t raw){
    double seconds = (double)raw*TIME_LSB;
    snprintf(string, size, "\"%f m\"", seconds/60);
}
// TTE
// QRTable00
// FullSOCThr
// RCell
// AvgTA
// Cycles
// DesignCap
void VoltageReg_to_str(char * string, size_t size, uint16_t raw){
    double voltage_V = CELL_VOLTAGE_LSB*raw;
    snprintf(string, size, "\"%f V\"", voltage_V);
}
void TemperatureReg_to_str(char * string, size_t size, uint16_t raw){
    double temperature_C = ((int16_t)raw)*TEMPERATURE_LSB;
    snprintf(string, size, "\"%f C\"", temperature_C);
}
void CurrentReg_to_str(char * string, size_t size, uint16_t raw){
    double rsense_mOhm = get_rsense_mOhm();
    // LSB is 1.5625 uV across the sense resistor; uV / mOhm == mA
    double current_uV = CURRENT_LSB*(int16_t)raw;
    snprintf(string, size, "\"%f mA\"", current_uV/rsense_mOhm);
}

void PowerReg_to_str(char * string, size_t size, uint16_t raw){
    double rsense_mOhm = get_rsense_mOhm();
    // LSB is 8.0 uV^2 across the sense resistor; uV^2 / mOhm == mW
    double power_uV2 = POWER_LSB*(int16_t)raw;
    snprintf(string, size, "\"%f mW\"", power_uV2/rsense_mOhm);
}

void ResistanceReg_to_str(char * string, size_t size, uint16_t raw){
    double r_ohms = ((double)raw)*RESISTANCE_LSB;
    snprintf(string, size, "\"%f mOhm\"", r_ohms*1000);
}

void Config2_to_str(char * string, size_t size, uint16_t raw){
    char* flags [16] = {};
    int i = 0;
    if(_RSHIFT(raw, 15, 1)) flags[i++] = "POR_CMD";
    if(_RSHIFT(raw, 13, 1)) flags[i++] = "AtRtEn";
    if(_RSHIFT(raw, 12, 1)) flags[i++] = "ADCFIFOen";
    if(_RSHIFT(raw, 7, 1)) flags[i++] = "dSOCen";
    if(_RSHIFT(raw, 6, 1)) flags[i++] = "TAlrtEn";

    double power_time_s = (double)(45*(1 << _RSHIFT(raw, 8, 4)))/(double)(1 << 6);
    double drcfg_s = (double)0.8*_RSHIFT(raw, 2, 2);

    char flags_str[256];
    flags_to_json(flags_str, sizeof(flags_str), flags, i);

    snprintf(string, size, "{\"flags\": %s, \"POWR\": \"%.2f s\", \"DRCfg\": \"%.1f to %.1f hr\"}",
             flags_str, power_time_s, drcfg_s, drcfg_s + 0.8);
}

void ProtAlrt_to_str(char * string, size_t size, uint16_t raw) {
    char* flags [16] = {};
    int i = 0;
    if(_RSHIFT(raw, 0, 1)) flags[i++] = "LDet";
    if(_RSHIFT(raw, 1, 1)) flags[i++] = "ResDFault";
    if(_RSHIFT(raw, 2, 1)) flags[i++] = "ODCP";
    if(_RSHIFT(raw, 3, 1)) flags[i++] = "UVP";
    if(_RSHIFT(raw, 4, 1)) flags[i++] = "TooHotD";
    if(_RSHIFT(raw, 5, 1)) flags[i++] = "DieHot";
    if(_RSHIFT(raw, 6, 1)) flags[i++] = "PermFail";
    if(_RSHIFT(raw, 7, 1)) flags[i++] = "Imbalance";
    if(_RSHIFT(raw, 8, 1)) flags[i++] = "PreqF";
    if(_RSHIFT(raw, 9, 1)) flags[i++] = "Qovflw";
    if(_RSHIFT(raw, 10, 1)) flags[i++] = "OCCP";
    if(_RSHIFT(raw, 11, 1)) flags[i++] = "OVP";
    if(_RSHIFT(raw, 12, 1)) flags[i++] = "TooColdC";
    if(_RSHIFT(raw, 13, 1)) flags[i++] = "Full";
    if(_RSHIFT(raw, 14, 1)) flags[i++] = "TooHotC";
    if(_RSHIFT(raw, 15, 1)) flags[i++] = "ChgWDT";

    flags_to_json(string, size, flags, i);
}

void Status2_to_str(char * string, size_t size, uint16_t raw) {
    if(_RSHIFT(raw, 0, 1) == 0) {
        strncpy(string, "nil", size);
    }
    else {
        strncpy(string, "\"Hib\"", size);
    }
}

void ProtStatus_to_str(char * string, size_t size, uint16_t raw){
    char* flags [16] = {};
    int i = 0;
    if(_RSHIFT(raw, 0, 1)) flags[i++] = "Ship";
    if(_RSHIFT(raw, 1, 1)) flags[i++] = "ResDFault";
    if(_RSHIFT(raw, 2, 1)) flags[i++] = "ODCP";
    if(_RSHIFT(raw, 3, 1)) flags[i++] = "UVP";
    if(_RSHIFT(raw, 4, 1)) flags[i++] = "TooHotD";
    if(_RSHIFT(raw, 5, 1)) flags[i++] = "DieHot";
    if(_RSHIFT(raw, 6, 1)) flags[i++] = "PermFail";
    if(_RSHIFT(raw, 7, 1)) flags[i++] = "Imbalance";
    if(_RSHIFT(raw, 8, 1)) flags[i++] = "PreqF";
    if(_RSHIFT(raw, 9, 1)) flags[i++] = "Qovflw";
    if(_RSHIFT(raw, 10, 1)) flags[i++] = "OCCP";
    if(_RSHIFT(raw, 11, 1)) flags[i++] = "OVP";
    if(_RSHIFT(raw, 12, 1)) flags[i++] = "TooColdC";
    if(_RSHIFT(raw, 13, 1)) flags[i++] = "Full";
    if(_RSHIFT(raw, 14, 1)) flags[i++] = "TooHotC";
    if(_RSHIFT(raw, 15, 1)) flags[i++] = "ChgWDT";

    flags_to_json(string, size, flags, i);
}

void Batt_to_srt(char * string, size_t size, uint16_t raw) {
    double voltage_V = PACK_VOLTAGE_LSB*raw;
    snprintf(string, size, "\"%f V\"", voltage_V);
}

void nConfig_to_str(char * string, size_t size, uint16_t raw){
    char* flags [16] = {};
    int i = 0;
    if(_RSHIFT(raw, 15, 1)) flags[i++] = "PAen";
    if(_RSHIFT(raw, 14, 1)) flags[i++] = "SS";
    if(_RSHIFT(raw, 13, 1)) flags[i++] = "TS";
    if(_RSHIFT(raw, 12, 1)) flags[i++] = "VS";
    if(_RSHIFT(raw, 11, 1)) flags[i++] = "FIFOen";
    if(_RSHIFT(raw, 10, 1)) flags[i++] = "PBen";
    if(_RSHIFT(raw, 9, 1)) flags[i++] = "DisBlockRead";
    if(_RSHIFT(raw, 7, 1)) flags[i++] = "AtRateEn";
    if(_RSHIFT(raw, 6, 1)) flags[i++] = "COMMSH";
    if(_RSHIFT(raw, 5, 1)) flags[i++] = "ALSH";
    if(_RSHIFT(raw, 3, 1)) flags[i++] = "FTHRM";
    if(_RSHIFT(raw, 2, 1)) flags[i++] = "Aen";
    if(_RSHIFT(raw, 1, 1)) flags[i++] = "dSOCen";
    if(_RSHIFT(raw, 0, 1)) flags[i++] = "TAlrtEn";

    flags_to_json(string, size, flags, i);
}

void nRSense_to_str(char * string, size_t size, uint16_t raw) {
    double resistance_mOhm = 0.01 * raw;
    snprintf(string, size, "\"%.2f mOhm\"", resistance_mOhm);
}


RegisterDescription desc[] = {
    {.name = "Status",      .address = 0x0000, .from_raw = simple_get, .value_2_str = Status_to_str},
    {.name = "VAlrtTh",     .address = 0x0001, .from_raw = simple_get, .value_2_str = VAlrtTh_to_str},
    {.name = "TAlrtTh",     .address = 0x0002, .from_raw = simple_get, .value_2_str = TAlrtTh_to_str},
    {.name = "SAlrtTh",     .address = 0x0003, .from_raw = simple_get, .value_2_str = SAlrtTh_to_str},
    {.name = "AtRate",      .address = 0x0004, .from_raw = simple_get, .value_2_str = CurrentReg_to_str},
    {.name = "RepCap",      .address = 0x0005, .from_raw = simple_get, .value_2_str = CapacityReg_to_str},
    {.name = "RepSOC",      .address = 0x0006, .from_raw = simple_get, .value_2_str = PercentageReg_to_str},
    {.name = "Age",         .address = 0x0007, .from_raw = simple_get, .value_2_str = PercentageReg_to_str},
    {.name = "MaxMinVolt",  .address = 0x0008, .from_raw = simple_get, .value_2_str = MaxMinVolt_to_str},
    {.name = "MaxMinTemp",  .address = 0x0009, .from_raw = simple_get, .value_2_str = MaxMinTemp_to_str},
    {.name = "MaxMinCurr",  .address = 0x000A, .from_raw = simple_get, .value_2_str = MaxMinCurr_to_str},
    {.name = "Config",      .address = 0x000B, .from_raw = simple_get, .value_2_str = Config_to_str},
    {.name = "QResidual",   .address = 0x000C, .from_raw = simple_get, .value_2_str = CapacityReg_to_str},
    {.name = "MixSOC",      .address = 0x000D, .from_raw = simple_get, .value_2_str = PercentageReg_to_str},
    {.name = "AvSOC",       .address = 0x000E, .from_raw = simple_get, .value_2_str = PercentageReg_to_str},
    {.name = "MiscCfg",     .address = 0x000F, .from_raw = simple_get, .value_2_str = test_parse},

    {.name = "FullCapRep",  .address = 0x0010, .from_raw = simple_get, .value_2_str = CapacityReg_to_str},
    {.name = "TTE",         .address = 0x0011, .from_raw = simple_get, .value_2_str = TimeReg_to_str},
    {.name = "QRTable00",   .address = 0x0012, .from_raw = simple_get, .value_2_str = test_parse},
    {.name = "FullSOCThr",  .address = 0x0013, .from_raw = simple_get, .value_2_str = PercentageReg_to_str},
    {.name = "RCell",       .address = 0x0014, .from_raw = simple_get, .value_2_str = ResistanceReg_to_str},
    {.name = "AvgTA",       .address = 0x0016, .from_raw = simple_get, .value_2_str = TemperatureReg_to_str},
    {.name = "Cycles",      .address = 0x0017, .from_raw = simple_get, .value_2_str = test_parse},
    {.name = "DesignCap",   .address = 0x0018, .from_raw = simple_get, .value_2_str = CapacityReg_to_str},
    {.name = "AvgVCell",    .address = 0x0019, .from_raw = simple_get, .value_2_str = VoltageReg_to_str},
    {.name = "VCell",       .address = 0x001A, .from_raw = simple_get, .value_2_str = VoltageReg_to_str},
    {.name = "Temp",        .address = 0x001B, .from_raw = simple_get, .value_2_str = TemperatureReg_to_str},
    {.name = "Current",     .address = 0x001C, .from_raw = simple_get, .value_2_str = CurrentReg_to_str},
    {.name = "AvgCurrent",  .address = 0x001D, .from_raw = simple_get, .value_2_str = CurrentReg_to_str},
    {.name = "IChgTerm",    .address = 0x001E, .from_raw = simple_get, .value_2_str = test_parse},
    {.name = "AvCap",       .address = 0x001F, .from_raw = simple_get, .value_2_str = CapacityReg_to_str},

    {.name = "TTF",             .address = 0x0020, .from_raw = simple_get, .value_2_str = TimeReg_to_str},
    {.name = "DevName",         .address = 0x0021, .from_raw = simple_get, .value_2_str = test_parse},
    {.name = "QRTable10",       .address = 0x0022, .from_raw = simple_get, .value_2_str = test_parse},
    {.name = "FullCapNom",      .address = 0x0023, .from_raw = simple_get, .value_2_str = test_parse},
    {.name = "ChargingCurrent", .address = 0x0028, .from_raw = simple_get, .value_2_str = CurrentReg_to_str},
    {.name = "FilterCfg",       .address = 0x0029, .from_raw = simple_get, .value_2_str = test_parse},
    {.name = "ChargingVoltage", .address = 0x002A, .from_raw = simple_get, .value_2_str = VoltageReg_to_str},
    {.name = "MixCap",          .address = 0x002B, .from_raw = simple_get, .value_2_str = CapacityReg_to_str},

    {.name = "QRTable20",   .address = 0x0032, .from_raw = simple_get, .value_2_str = test_parse},
    {.name = "DieTemp",     .address = 0x0034, .from_raw = simple_get, .value_2_str = TemperatureReg_to_str},
    {.name = "FullCap",     .address = 0x0035, .from_raw = simple_get, .value_2_str = CapacityReg_to_str},
    {.name = "IAvgEmpty",   .address = 0x0036, .from_raw = simple_get, .value_2_str = test_parse},
    {.name = "VEmpty",      .address = 0x003A, .from_raw = simple_get, .value_2_str = test_parse},
    {.name = "FStat",       .address = 0x003D, .from_raw = simple_get, .value_2_str = test_parse},
    {.name = "Timer",       .address = 0x003E, .from_raw = simple_get, .value_2_str = test_parse},

    {.name = "AvgDieTemp",  .address = 0x0040, .from_raw = simple_get, .value_2_str = TemperatureReg_to_str},
    {.name = "QRTable30 ",  .address = 0x0042, .from_raw = simple_get, .value_2_str = test_parse},
    {.name = "VFRemCap",    .address = 0x004A, .from_raw = simple_get, .value_2_str = CapacityReg_to_str},
    {.name = "QH",          .address = 0x004D, .from_raw = simple_get, .value_2_str = CapacityReg_to_str},
    {.name = "QL",          .address = 0x004E, .from_raw = simple_get, .value_2_str = test_parse},

    {.name = "RelaxCfg",        .address = 0x00A0, .from_raw = simple_get, .value_2_str = test_parse},
    {.name = "LearnCfg",        .address = 0x00A1, .from_raw = simple_get, .value_2_str = test_parse},
    {.name = "MaxPeakPower",    .address = 0x00A4, .from_raw = simple_get, .value_2_str = PowerReg_to_str},
    {.name = "SusPeakPower",    .address = 0x00A5, .from_raw = simple_get, .value_2_str = PowerReg_to_str},
    {.name = "PackResistance",  .address = 0x00A6, .from_raw = simple_get, .value_2_str = test_parse},
    {.name = "SysResistance",   .address = 0x00A7, .from_raw = simple_get, .value_2_str = test_parse},
    {.name = "MinSysVoltage",   .address = 0x00A8, .from_raw = simple_get, .value_2_str = test_parse},
    {.name = "MPPCurrent",      .address = 0x00A9, .from_raw = simple_get, .value_2_str = test_parse},
    {.name = "SPPCurrent",      .address = 0x00AA, .from_raw = simple_get, .value_2_str = test_parse},
    {.name = "Config2",         .address = 0x00AB, .from_raw = simple_get, .value_2_str = Config2_to_str},
    {.name = "IAlrtTh",         .address = 0x00AC, .from_raw = simple_get, .value_2_str = test_parse},
    {.name = "MinVolt",         .address = 0x00AD, .from_raw = simple_get, .value_2_str = test_parse},
    {.name = "MinCurr",         .address = 0x00AE, .from_raw = simple_get, .value_2_str = test_parse},
    {.name = "ProtAlrt",        .address = 0x00AF, .from_raw = simple_get, .value_2_str = ProtAlrt_to_str},

    {.name = "Status2",     .address = 0x00B0, .from_raw = simple_get, .value_2_str = Status2_to_str},
    {.name = "Power",       .address = 0x00B1, .from_raw = simple_get, .value_2_str = PowerReg_to_str},
    {.name = "VRipple",     .address = 0x00B2, .from_raw = simple_get, .value_2_str = test_parse},
    {.name = "AvgPower",    .address = 0x00B3, .from_raw = simple_get, .value_2_str = PowerReg_to_str},
    {.name = "TTFCfg",      .address = 0x00B5, .from_raw = simple_get, .value_2_str = test_parse},
    {.name = "CVMixCap",    .address = 0x00B6, .from_raw = simple_get, .value_2_str = test_parse},
    {.name = "CVHalfTime",  .address = 0x00B7, .from_raw = simple_get, .value_2_str = test_parse},
    {.name = "CGTempCo",    .address = 0x00B8, .from_raw = simple_get, .value_2_str = test_parse},
    {.name = "AgeForecast", .address = 0x00B9, .from_raw = simple_get, .value_2_str = test_parse},
    {.name = "TimerH",      .address = 0x00BE, .from_raw = simple_get, .value_2_str = test_parse},

    {.name = "SOCHold",     .address = 0x00D0, .from_raw = simple_get, .value_2_str = test_parse},
    {.name = "AvgCell4",    .address = 0x00D1, .from_raw = simple_get, .value_2_str = VoltageReg_to_str},
    {.name = "AvgCell3",    .address = 0x00D2, .from_raw = simple_get, .value_2_str = VoltageReg_to_str},
    {.name = "AvgCell2",    .address = 0x00D3, .from_raw = simple_get, .value_2_str = VoltageReg_to_str},
    {.name = "AvgCell1",    .address = 0x00D4, .from_raw = simple_get, .value_2_str = VoltageReg_to_str},
    {.name = "CELL4",       .address = 0x00D5, .from_raw = simple_get, .value_2_str = VoltageReg_to_str},
    {.name = "CELL3",       .address = 0x00D6, .from_raw = simple_get, .value_2_str = VoltageReg_to_str},
    {.name = "CELL2",       .address = 0x00D7, .from_raw = simple_get, .value_2_str = VoltageReg_to_str},
    {.name = "CELL1",       .address = 0x00D8, .from_raw = simple_get, .value_2_str = VoltageReg_to_str},
    {.name = "ProtStatus",  .address = 0x00D9, .from_raw = simple_get, .value_2_str = ProtStatus_to_str},
    {.name = "Batt",        .address = 0x00DA, .from_raw = simple_get, .value_2_str = Batt_to_srt},
    {.name = "PCKP",        .address = 0x00DB, .from_raw = simple_get, .value_2_str = Batt_to_srt},
    {.name = "AtQResidual", .address = 0x00DC, .from_raw = simple_get, .value_2_str = CapacityReg_to_str},
    {.name = "AtTTE",       .address = 0x00DD, .from_raw = simple_get, .value_2_str = TimeReg_to_str},
    {.name = "AtAvSOC",     .address = 0x00DE, .from_raw = simple_get, .value_2_str = PercentageReg_to_str},
    {.name = "AtAVCap",     .address = 0x00DF, .from_raw = simple_get, .value_2_str = CapacityReg_to_str},

    {.name = "VFOCV",     .address = 0x00FB, .from_raw = simple_get, .value_2_str = VoltageReg_to_str},
    {.name = "VFSOC",     .address = 0x00FF, .from_raw = simple_get, .value_2_str = PercentageReg_to_str},

    {.name = "AvgTemp4",     .address = 0x0133, .from_raw = simple_get, .value_2_str = TemperatureReg_to_str},
    {.name = "AvgTemp3",     .address = 0x0134, .from_raw = simple_get, .value_2_str = TemperatureReg_to_str},
    {.name = "AvgTemp2",     .address = 0x0135, .from_raw = simple_get, .value_2_str = TemperatureReg_to_str},
    {.name = "AvgTemp1",     .address = 0x0136, .from_raw = simple_get, .value_2_str = TemperatureReg_to_str},
    {.name = "Temp4",     .address = 0x0137, .from_raw = simple_get, .value_2_str = TemperatureReg_to_str},
    {.name = "Temp3",     .address = 0x0138, .from_raw = simple_get, .value_2_str = TemperatureReg_to_str},
    {.name = "Temp2",     .address = 0x0139, .from_raw = simple_get, .value_2_str = TemperatureReg_to_str},
    {.name = "Temp1",     .address = 0x013A, .from_raw = simple_get, .value_2_str = TemperatureReg_to_str},

    {.name = "nVAlrtTh",    .address = 0x018C, .from_raw = simple_get, .value_2_str = VAlrtTh_to_str},
    {.name = "nTAlrtTh",    .address = 0x018D, .from_raw = simple_get, .value_2_str = TAlrtTh_to_str},
    {.name = "nIAlrtTh",    .address = 0x018E, .from_raw = simple_get, .value_2_str = test_parse},
    {.name = "nSAlrtTh",    .address = 0x018F, .from_raw = simple_get, .value_2_str = SAlrtTh_to_str},

    // {.name = "nOCVTable0",  .address = 0x0190, .from_raw = simple_get, .value_2_str = test_parse},
    // {.name = "nOCVTable1",  .address = 0x0191, .from_raw = simple_get, .value_2_str = test_parse},
    // {.name = "nOCVTable2",  .address = 0x0192, .from_raw = simple_get, .value_2_str = test_parse},
    // {.name = "nOCVTable3",  .address = 0x0193, .from_raw = simple_get, .value_2_str = test_parse},
    // {.name = "nOCVTable4",  .address = 0x0194, .from_raw = simple_get, .value_2_str = test_parse},
    // {.name = "nOCVTable5",  .address = 0x0195, .from_raw = simple_get, .value_2_str = test_parse},
    // {.name = "nOCVTable6",  .address = 0x0196, .from_raw = simple_get, .value_2_str = test_parse},
    // {.name = "nOCVTable7",  .address = 0x0197, .from_raw = simple_get, .value_2_str = test_parse},
    // {.name = "nOCVTable8",  .address = 0x0198, .from_raw = simple_get, .value_2_str = test_parse},
    // {.name = "nOCVTable9",  .address = 0x0199, .from_raw = simple_get, .value_2_str = test_parse},
    // {.name = "nOCVTable10", .address = 0x019A, .from_raw = simple_get, .value_2_str = test_parse},
    // {.name = "nOCVTable11", .address = 0x019B, .from_raw = simple_get, .value_2_str = test_parse},
    {.name = "nChgTerm",    .address = 0x019C, .from_raw = simple_get, .value_2_str = CurrentReg_to_str},
    {.name = "nFilterCfg",  .address = 0x019D, .from_raw = simple_get, .value_2_str = test_parse},
    {.name = "nVEmpty",     .address = 0x019E, .from_raw = simple_get, .value_2_str = test_parse},
    {.name = "nLearnCfg",   .address = 0x019F, .from_raw = simple_get, .value_2_str = test_parse},

    // {.name = "nQRTable00",  .address = 0x01A0, .from_raw = simple_get, .value_2_str = test_parse},
    // {.name = "nQRTable01",  .address = 0x01A1, .from_raw = simple_get, .value_2_str = test_parse},
    // {.name = "nQRTable02",  .address = 0x01A2, .from_raw = simple_get, .value_2_str = test_parse},
    // {.name = "nQRTable03",  .address = 0x01A3, .from_raw = simple_get, .value_2_str = test_parse},
    {.name = "nCycles",     .address = 0x01A4, .from_raw = simple_get, .value_2_str = test_parse},
    {.name = "nFullCapNom", .address = 0x01A5, .from_raw = simple_get, .value_2_str = CapacityReg_to_str},
    {.name = "nRComp0",     .address = 0x01A6, .from_raw = simple_get, .value_2_str = test_parse},
    {.name = "nTempCo",     .address = 0x01A7, .from_raw = simple_get, .value_2_str = test_parse},
    {.name = "nBattStatus", .address = 0x01A8, .from_raw = simple_get, .value_2_str = test_parse},
    {.name = "nFullCapRep", .address = 0x01A9, .from_raw = simple_get, .value_2_str = test_parse},
    {.name = "nVoltTemp",   .address = 0x01AA, .from_raw = simple_get, .value_2_str = test_parse},
    {.name = "nMaxMinCurr", .address = 0x01AB, .from_raw = simple_get, .value_2_str = MaxMinCurr_to_str},
    {.name = "nMaxMinVolt", .address = 0x01AC, .from_raw = simple_get, .value_2_str = MaxMinVolt_to_str},
    {.name = "nMaxMinTemp", .address = 0x01AD, .from_raw = simple_get, .value_2_str = MaxMinTemp_to_str},
    {.name = "nFaultLog",   .address = 0x01AE, .from_raw = simple_get, .value_2_str = test_parse},
    {.name = "nFullCapFlt", .address = 0x01AE, .from_raw = simple_get, .value_2_str = test_parse},
    {.name = "nTimerH",     .address = 0x01AF, .from_raw = simple_get, .value_2_str = test_parse},

    {.name = "nConfig",         .address = 0x01B0, .from_raw = simple_get, .value_2_str = nConfig_to_str},
    {.name = "nRippleCfg",      .address = 0x01B1, .from_raw = simple_get, .value_2_str = test_parse},
    {.name = "nMiscCfg",        .address = 0x01B2, .from_raw = simple_get, .value_2_str = test_parse},
    {.name = "nDesignCap",      .address = 0x01B3, .from_raw = simple_get, .value_2_str = CapacityReg_to_str},
    {.name = "nSBSCfg",         .address = 0x01B4, .from_raw = simple_get, .value_2_str = test_parse},
    {.name = "nPackCfg",        .address = 0x01B5, .from_raw = simple_get, .value_2_str = nPackCfg_to_str},
    {.name = "nRelaxCfg",       .address = 0x01B6, .from_raw = simple_get, .value_2_str = test_parse},
    {.name = "nConvgCfg",       .address = 0x01B7, .from_raw = simple_get, .value_2_str = test_parse},
    {.name = "nNVCCfg0",        .address = 0x01B8, .from_raw = simple_get, .value_2_str = test_parse},
    {.name = "nNVCCfg1",        .address = 0x01B9, .from_raw = simple_get, .value_2_str = test_parse},
    {.name = "nNVCCfg2",        .address = 0x01BA, .from_raw = simple_get, .value_2_str = test_parse},
    {.name = "nHibCfg",         .address = 0x01BB, .from_raw = simple_get, .value_2_str = test_parse},
    {.name = "nROMID0",         .address = 0x01BC, .from_raw = simple_get, .value_2_str = test_parse},
    {.name = "nROMID1",         .address = 0x01BD, .from_raw = simple_get, .value_2_str = test_parse},
    {.name = "nROMID2",         .address = 0x01BE, .from_raw = simple_get, .value_2_str = test_parse},
    {.name = "nROMID3",         .address = 0x01BF, .from_raw = simple_get, .value_2_str = test_parse},
    
    {.name = "nChgCfg",         .address = 0x01C2, .from_raw = simple_get, .value_2_str = test_parse},
    {.name = "nChgCtrl",        .address = 0x01C3, .from_raw = simple_get, .value_2_str = test_parse},
    {.name = "nRGain",          .address = 0x01C4, .from_raw = simple_get, .value_2_str = test_parse},
    {.name = "nPackResistance", .address = 0x01C5, .from_raw = simple_get, .value_2_str = test_parse},
    {.name = "nFullSOCThr",     .address = 0x01C6, .from_raw = simple_get, .value_2_str = PercentageReg_to_str},
    {.name = "nTTFCfg",         .address = 0x01C7, .from_raw = simple_get, .value_2_str = test_parse},
    {.name = "nCGain",          .address = 0x01C8, .from_raw = simple_get, .value_2_str = test_parse},
    {.name = "nCGTempCo",       .address = 0x01C9, .from_raw = simple_get, .value_2_str = test_parse},
    {.name = "nThermCfg",       .address = 0x01CA, .from_raw = simple_get, .value_2_str = test_parse},
    {.name = "nManfctrName0",   .address = 0x01CC, .from_raw = simple_get, .value_2_str = test_parse},
    {.name = "nManfctrName1",   .address = 0x01CD, .from_raw = simple_get, .value_2_str = test_parse},
    {.name = "nManfctrName2",   .address = 0x01CE, .from_raw = simple_get, .value_2_str = test_parse},
    {.name = "nRSense",         .address = 0x01CF, .from_raw = simple_get, .value_2_str = nRSense_to_str},
    
    {.name = "nUVPrtTh",    .address = 0x01D0, .from_raw = simple_get, .value_2_str = test_parse},
    {.name = "nTPrtTh1",    .address = 0x01D1, .from_raw = simple_get, .value_2_str = test_parse},
    {.name = "nTPrtTh3",    .address = 0x01D2, .from_raw = simple_get, .value_2_str = test_parse},
    {.name = "nlPrtTh1",    .address = 0x01D3, .from_raw = simple_get, .value_2_str = test_parse},
    {.name = "nBALTh",      .address = 0x01D4, .from_raw = simple_get, .value_2_str = test_parse},
    {.name = "nTPrtTh2",    .address = 0x01D5, .from_raw = simple_get, .value_2_str = test_parse},
    {.name = "nProtMiscTh", .address = 0x01D6, .from_raw = simple_get, .value_2_str = test_parse},
    {.name = "nProtCfg",    .address = 0x01D7, .from_raw = simple_get, .value_2_str = test_parse},
    {.name = "nJEITAC",     .address = 0x01D8, .from_raw = simple_get, .value_2_str = test_parse},
    {.name = "nJEITAV",     .address = 0x01D9, .from_raw = simple_get, .value_2_str = test_parse},
    {.name = "nOVPrtTh",    .address = 0x01DA, .from_raw = simple_get, .value_2_str = test_parse},
    {.name = "nStepChg",    .address = 0x01DB, .from_raw = simple_get, .value_2_str = test_parse},
    {.name = "nDelayChg",   .address = 0x01DC, .from_raw = simple_get, .value_2_str = test_parse},
    {.name = "nODSCTh",     .address = 0x01DD, .from_raw = simple_get, .value_2_str = test_parse},
    {.name = "nODSCCfg",    .address = 0x01DE, .from_raw = simple_get, .value_2_str = test_parse},
    {.name = "nProtCfg2",   .address = 0x01DF, .from_raw = simple_get, .value_2_str = test_parse},
    
    {.name = "nDPLimit",        .address = 0x01E0, .from_raw = simple_get, .value_2_str = test_parse},
    {.name = "nScOcvLim",       .address = 0x01E1, .from_raw = simple_get, .value_2_str = test_parse},
    {.name = "nAgeFcCfg",       .address = 0x01E2, .from_raw = simple_get, .value_2_str = nAgeFcCfg_to_str},
    {.name = "nFirtUsed",       .address = 0x01E3, .from_raw = simple_get, .value_2_str = test_parse},
    {.name = "nManfctrDate",    .address = 0x01E6, .from_raw = simple_get, .value_2_str = test_parse},
    {.name = "nFirstUsed",      .address = 0x01E7, .from_raw = simple_get, .value_2_str = test_parse},
    {.name = "nSerialNumber0",  .address = 0x01E8, .from_raw = simple_get, .value_2_str = test_parse},
    {.name = "nSerialNumber1",  .address = 0x01E9, .from_raw = simple_get, .value_2_str = test_parse},
    {.name = "nSerialNumber2",  .address = 0x01EA, .from_raw = simple_get, .value_2_str = test_parse},
    {.name = "nDeviceName0",    .address = 0x01EB, .from_raw = simple_get, .value_2_str = test_parse},
    {.name = "nDeviceName1",    .address = 0x01EC, .from_raw = simple_get, .value_2_str = test_parse},
    {.name = "nDeviceName2",    .address = 0x01ED, .from_raw = simple_get, .value_2_str = test_parse},
    {.name = "nDeviceName3",    .address = 0x01EE, .from_raw = simple_get, .value_2_str = test_parse},
    {.name = "nDeviceName4",    .address = 0x01EE, .from_raw = simple_get, .value_2_str = test_parse},
    {.name = "nDeviceName5",    .address = 0x01EF, .from_raw = simple_get, .value_2_str = test_parse},
    
    {.name = "HProtCfg2",       .address = 0x01F1, .from_raw = simple_get, .value_2_str = test_parse},
};


void print_description(RegisterDescription *reg){
    char desc_str[512] = "nil";
    uint16_t value = 0;
    reg->from_raw(reg, &value);
    reg->value_2_str(desc_str, sizeof(desc_str), value);
    printf("\"(%03x) %s\": %s", reg->address, reg->name, desc_str);
}

// Resolves an argument to an index into desc[], either as a numeric address
// (decimal, or hex with a 0x prefix) or as a register name. Returns -1 if the
// argument matches neither.
static int find_register(const char *arg){
    char *endptr;
    long addr = strtol(arg, &endptr, 0);
    // Only treat the argument as an address when the whole string parses as
    // one; a partial parse would silently resolve e.g. "1CF" to address 0x001.
    if(endptr != arg && *endptr == '\0'){
        for(size_t j = 0; j < sizeof(desc)/sizeof(*desc); j++){
            if(desc[j].address == addr){
                return (int)j;
            }
        }
        return -1;
    }
    for(size_t j = 0; j < sizeof(desc)/sizeof(*desc); j++){
        if(strcasecmp(arg, desc[j].name) == 0){
            return (int)j;
        }
    }
    return -1;
}

#define DEFAULT_POLL_INTERVAL_NS 1000000000LL // 1 Hz
#define NS_PER_S                 1000000000LL

// Set from a signal handler so that a polling run can be stopped with ctrl-c
// without losing the row currently being written.
static volatile sig_atomic_t stop_requested = 0;

static void handle_stop(int sig){
    (void)sig;
    stop_requested = 1;
}

// The unit a converter appends to its value, or NULL for the composite and
// flag converters, whose output has no single unit. Units belong to the
// converter rather than to the register, so one entry covers every register
// that shares a converter.
static const char * register_units(const RegisterDescription *reg){
    if(reg->value_2_str == CurrentReg_to_str)       return "mA";
    if(reg->value_2_str == PowerReg_to_str)         return "mW";
    if(reg->value_2_str == CapacityReg_to_str)      return "mAh";
    if(reg->value_2_str == VoltageReg_to_str)       return "V";
    if(reg->value_2_str == Batt_to_srt)             return "V";
    if(reg->value_2_str == TemperatureReg_to_str)   return "C";
    if(reg->value_2_str == PercentageReg_to_str)    return "%";
    if(reg->value_2_str == TimeReg_to_str)          return "m";
    if(reg->value_2_str == ResistanceReg_to_str)    return "mOhm";
    if(reg->value_2_str == nRSense_to_str)          return "mOhm";
    return NULL;
}

// Strips the double quotes the converters wrap scalar values in for json.
static void unquote(char *value){
    size_t len = strlen(value);
    if(len < 2 || value[0] != '"' || value[len - 1] != '"'){
        return;
    }
    memmove(value, value + 1, len - 2);
    value[len - 2] = '\0';
}

// Drops the trailing unit from a value, so that it can be named once in the
// header instead of being repeated on every row.
static void strip_units(char *value, const char *units){
    if(units == NULL){
        return;
    }
    size_t vlen = strlen(value);
    size_t ulen = strlen(units);
    if(vlen <= ulen || strcmp(value + vlen - ulen, units) != 0){
        return;
    }
    size_t end = vlen - ulen;
    // percentages are printed without a separating space
    if(end > 0 && value[end - 1] == ' '){
        end--;
    }
    value[end] = '\0';
}

// Writes one CSV field, escaped per RFC 4180.
static void print_csv_field(const char *value){
    if(strpbrk(value, ",\"\r\n") == NULL){
        printf("%s", value);
        return;
    }
    putchar('"');
    for(const char *p = value; *p != '\0'; p++){
        if(*p == '"'){
            putchar('"');
        }
        putchar(*p);
    }
    putchar('"');
}

static void print_json(const int *selected, int n){
    printf("{\n");
    for(int i = 0; i < n; i++){
        print_description(&desc[selected[i]]);
        if(i != n - 1)
            printf(",\n");
    }
    printf("\n}\n");
}

static void print_csv_header(const int *selected, int n){
    for(int i = 0; i < n; i++){
        const RegisterDescription *reg = &desc[selected[i]];
        const char *units = register_units(reg);
        char label[128];

        if(units != NULL){
            snprintf(label, sizeof(label), "%s (%s)", reg->name, units);
        } else {
            snprintf(label, sizeof(label), "%s", reg->name);
        }
        if(i != 0)
            printf(",");
        print_csv_field(label);
    }
    printf("\n");
}

static void print_csv_row(const int *selected, int n){
    for(int i = 0; i < n; i++){
        RegisterDescription *reg = &desc[selected[i]];
        char desc_str[512] = "nil";
        uint16_t value = 0;

        reg->from_raw(reg, &value);
        reg->value_2_str(desc_str, sizeof(desc_str), value);
        unquote(desc_str);
        strip_units(desc_str, register_units(reg));
        if(i != 0)
            printf(",");
        print_csv_field(desc_str);
    }
    printf("\n");
}

static void timespec_advance(struct timespec *ts, long long interval_ns){
    ts->tv_sec += (time_t)(interval_ns/NS_PER_S);
    ts->tv_nsec += (long)(interval_ns%NS_PER_S);
    if(ts->tv_nsec >= NS_PER_S){
        ts->tv_nsec -= NS_PER_S;
        ts->tv_sec += 1;
    }
}

// Sleeps until deadline. Returns early if a signal arrives, and does not sleep
// at all when the readings are already taking longer than the poll interval.
static void sleep_until(const struct timespec *deadline){
    struct timespec now, remaining;
    if(clock_gettime(CLOCK_MONOTONIC, &now) != 0){
        return;
    }
    remaining.tv_sec = deadline->tv_sec - now.tv_sec;
    remaining.tv_nsec = deadline->tv_nsec - now.tv_nsec;
    if(remaining.tv_nsec < 0){
        remaining.tv_sec -= 1;
        remaining.tv_nsec += NS_PER_S;
    }
    if(remaining.tv_sec < 0){
        return;
    }
    nanosleep(&remaining, NULL);
}

// Reads the selected registers once per interval until interrupted.
static void poll_csv(const int *selected, int n, long long interval_ns){
    struct timespec deadline;

    // take over the signals pigpio installs handlers for, so that the loop
    // unwinds through atexit(gpioTerminate) instead of exiting from a handler
    signal(SIGINT, handle_stop);
    signal(SIGTERM, handle_stop);

    print_csv_header(selected, n);
    clock_gettime(CLOCK_MONOTONIC, &deadline);
    while(!stop_requested){
        print_csv_row(selected, n);
        // rows are consumed as they are produced, so do not sit in the buffer
        fflush(stdout);
        timespec_advance(&deadline, interval_ns);
        sleep_until(&deadline);
    }
}

// Parses a poll interval in seconds. Returns 0 on success.
static int parse_interval(const char *arg, long long *interval_ns){
    char *endptr;
    double seconds = strtod(arg, &endptr);
    if(endptr == arg || *endptr != '\0' || !(seconds > 0.0)){
        return -1;
    }
    *interval_ns = (long long)(seconds*(double)NS_PER_S + 0.5);
    if(*interval_ns <= 0){
        return -1;
    }
    return 0;
}

MAX17320_HandleTypeDef dev;

int main(int argc, char* argv[]){
    size_t n_desc = sizeof(desc)/sizeof(*desc);
    int csv_output = 0;
    long long interval_ns = DEFAULT_POLL_INTERVAL_NS;
    int interval_given = 0;
    int clear_max_current = 0;
    int verify_nv = 0;
    int program_nv = 0;

    // room for either every register or every argument, whichever is larger
    int *selected = malloc((n_desc + (size_t)argc)*sizeof(*selected));
    if(selected == NULL){
        printf("Err: out of memory\n");
        return -1;
    }
    int n_selected = 0;

    //verify inputs as valid flags, numbers, or register names
    for(int i = 1; i < argc; i++){
        const char *interval_arg = NULL;

        if(strcmp(argv[i], "--csv") == 0){
            csv_output = 1;
            continue;
        }
        if(strcmp(argv[i], "--clear-max-current") == 0){
            clear_max_current = 1;
            continue;
        }
        if(strcmp(argv[i], "--verify-nv") == 0){
            verify_nv = 1;
            continue;
        }
        if(strcmp(argv[i], "--program-nv") == 0){
            program_nv = 1;
            continue;
        }
        if(strncmp(argv[i], "--interval=", 11) == 0){
            interval_arg = argv[i] + 11;
        } else if(strcmp(argv[i], "--interval") == 0){
            if(i + 1 >= argc){
                printf("Err: --interval requires a value in seconds\n");
                free(selected);
                return -1;
            }
            interval_arg = argv[++i];
        }
        if(interval_arg != NULL){
            if(parse_interval(interval_arg, &interval_ns) != 0){
                printf("Err: \"%s\" is not a valid poll interval in seconds\n", interval_arg);
                free(selected);
                return -1;
            }
            interval_given = 1;
            continue;
        }

        int j = find_register(argv[i]);
        if(j < 0){
            printf("Err: \"%s\" is not a valid flag, readable register address, or register name\n", argv[i]);
            printf("Usage: %s [--clear-max-current] [--csv [--interval SECONDS]] [register ...]\n", argv[0]);
            printf("       %s --verify-nv | --program-nv\n", argv[0]);
            free(selected);
            return -1;
        }
        selected[n_selected++] = j;
    }

    if(interval_given && !csv_output){
        printf("Err: --interval only applies to --csv output\n");
        free(selected);
        return -1;
    }

    // the nonvolatile memory modes print their own report, so they do not
    // combine with register readings
    if((verify_nv || program_nv) && (verify_nv + program_nv + csv_output + clear_max_current + (n_selected > 0)) > 1){
        printf("Err: --verify-nv and --program-nv cannot be combined with other flags or registers\n");
        free(selected);
        return -1;
    }
    if(verify_nv || program_nv){
        if (gpioInitialise() < 0) {
            free(selected);
            return -1;
        }
        atexit(gpioTerminate);
        free(selected);
        if(program_nv){
            return bms_ctl_program_nonvolatile_memory() == 0 ? 0 : -1;
        }
        return bms_ctl_verify() == 1 ? 0 : -1;
    }

    // naming no registers selects all of them
    if(n_selected == 0){
        for(size_t j = 0; j < n_desc; j++){
            selected[n_selected++] = (int)j;
        }
    }

    if (gpioInitialise() < 0) {
        free(selected);
        return -1;
    }
    atexit(gpioTerminate);

    // reset the recorded peaks before the first reading, so the run reports
    // only the current seen while it was running
    if(clear_max_current && max17320_clear_max_min_current(&dev) < 0){
        printf("Err: could not clear MaxMinCurr\n");
        free(selected);
        return -1;
    }

    if(csv_output){
        poll_csv(selected, n_selected, interval_ns);
    } else {
        print_json(selected, n_selected);
    }

    free(selected);
    return 0;
}
