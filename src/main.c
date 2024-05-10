#include <pigpio.h>
#include "max17320.h"
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

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

float get_rsense_mOhm(void) {
    uint16_t resense_raw;
    max17320_read(0x01CF, &resense_raw);
    float rsense = 0.01*resense_raw;
    return rsense;
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

    if(i == 0) {
        strncpy(string, "nil", size);
        return;
    }
    strncpy(string, "\"", size);
    for(int j = 0; j < i; j++){
        if (j != 0) {
            strncat(string, " | ", size);
        }
        strncat(string, flags[j], size);
    }
    strncat(string, "\"", size);
}
 
void VAlrtTh_to_str(char * string, size_t size, uint16_t raw){
    float vmax = 0.02*(raw>>8);
    float vmin = 0.02*(raw& 0xff);
    snprintf(string, size, "{\"VMAX\": \"%.2f v\", \"VMIN\": \"%.2f v\"}", vmax, vmin);
}

void TAlrtTh_to_str(char * string, size_t size, uint16_t raw){
    int8_t tmax = (raw >> 8) & 0xFF;
    int8_t tmin = (raw & 0xff);
    snprintf(string, size, "{\"TMAX\": \"%d C\", \"TMIN\": \"%d C\"}", raw, tmax, tmin);
}

void SAlrtTh_to_str(char * string, size_t size, uint16_t raw){
    uint8_t smax = (raw >> 8) & 0xFF;
    uint8_t smin = (raw & 0xff);
    snprintf(string, size, "{\"SMAX\": \"%d\%\", \"SMIN\": \"%d\%\"}", smax, smin);
}

void AtRate_to_str(char * string, size_t size, uint16_t raw){}
void RepCap_to_str(char * string, size_t size, uint16_t raw){}
void RepSOC_to_str(char * string, size_t size, uint16_t raw){}
void Age_to_str(char * string, size_t size, uint16_t raw){}
void MaxMinVolt_to_str(char * string, size_t size, uint16_t raw){
    float vmax = 0.02*(raw>>8);
    float vmin = 0.02*(raw& 0xff);
    snprintf(string, size, "{\"MaxVCell\": \"%.2f v\", \"MinVCell\": \"%.2f v\"}", vmax, vmin);
}
void MaxMinTemp_to_str(char * string, size_t size, uint16_t raw){
    int8_t tmax = (raw >> 8) & 0xFF;
    int8_t tmin = (raw & 0xff);
    snprintf(string, size, "{\"TMAX\": \"%d C\", \"TMIN\": \"%d C\"}", tmax, tmin);
}
void MaxMinCurr_to_str(char * string, size_t size, uint16_t raw){
    float rsense_Ohm = get_rsense_mOhm()/1000.0;
    float imax_V = 0.04*((int8_t) ((raw >> 8) & 0xFF));
    float imin_V = 0.04*((int8_t) (raw & 0xff));
    snprintf(string, size, "{\"MaxCurrent\": \"%f A\", \"MinCurrent\": \"%f A\"}", imax_V*rsense_Ohm, imin_V*rsense_Ohm);
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

    if(i == 0) {
        strncpy(string, "nil", size);
        return;
    }
    strncpy(string, "\"", size);
    for(int j = 0; j < i; j++){
        if (j != 0) {
            strncat(string, " | ", size);
        }
        strncat(string, flags[j], size);
    }
    strncat(string, "\"", size);
}


void CapacityReg_to_str(char * string, size_t size, uint16_t raw){
    float rsense_mOhm = get_rsense_mOhm();
    float voltage_uVh = 0.5*raw;
    snprintf(string, size, "\"%f mAh\"", voltage_uVh/rsense_mOhm);
}

void PercentageReg_to_str(char * string, size_t size, uint16_t raw){
    float percent = (float)raw/256.0;
    snprintf(string, size, "\"%.1f\%\"", percent);
}

void TimeReg_to_str(char * string, size_t size, uint16_t raw){
    float seconds = (float)raw*5.625;
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
    float voltage_mv = 0.078125*raw;
    snprintf(string, size, "\"%f V\"", voltage_mv/1000.0);
}
void TemperatureReg_to_str(char * string, size_t size, uint16_t raw){
    float temperature_C = ((int16_t)raw)/256.0;
    snprintf(string, size, "\"%f C\"", temperature_C);
}
void CurrentReg_to_str(char * string, size_t size, uint16_t raw){
    float rsense_mOhm = get_rsense_mOhm();
    float current_uV = 1.5625*(int16_t)raw;
    snprintf(string, size, "\"%f mA\"", current_uV/rsense_mOhm);
}

void ResistanceReg_to_str(char * string, size_t size, uint16_t raw){
    float r_ohms = ((float)raw)/4096.0;
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

    char power_str[512];
    float power_time_s = (float)(45*(1 << _RSHIFT(raw, 8, 4)))/(float)(1 << 6);

    char drcfg_str[512];
    float drcfg_s = (float)0.8*_RSHIFT(raw, 2, 2);   
    
    strncpy(string, "{\"flags\": ", size);
    if(i == 0) {
        strncat(string, "nil", size);
        return;
    }
    strncat(string, "\"", size);
    for(int j = 0; j < i; j++){
        if (j != 0) {
            strncat(string, " | ", size);
        }
        strncat(string, flags[j], size);
    }
    strncat(string, "\", ", size);
    snprintf(power_str, 512, "\"POWR\": \"%.2f s\", ", power_time_s);
    strncat(string, power_str, size);
    snprintf(drcfg_str, 512, "\"DRCfg\": \"%.1f to %.1f hr\"}", drcfg_s, drcfg_s + 0.8);
    strncat(string, drcfg_str, size);

}

void ProtAlrt_to_str(char * string, size_t size, uint16_t raw) {
    char* flags [16] = {};
    int i = 0;
    if(_RSHIFT(raw, 0, 1)) flags[i++] = "LDet";
    if(_RSHIFT(raw, 1, 1)) flags[i++] = "ResDefault";
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

    if(i == 0) {
        strncpy(string, "nil", size);
        return;
    }
    strncpy(string, "\"", size);
    for(int j = 0; j < i; j++){
        if (j != 0) {
            strncat(string, " | ", size);
        }
        strncat(string, flags[j], size);
    }
    strncat(string, "\"", size);
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

    if(i == 0) {
        strncpy(string, "nil", size);
        return;
    }
    strncpy(string, "\"", size);
    for(int j = 0; j < i; j++){
        if (j != 0) {
            strncat(string, " | ", size);
        }
        strncat(string, flags[j], size);
    }
    strncat(string, "\"", size);
}

void Batt_to_srt(char * string, size_t size, uint16_t raw) {
    float voltage_mv = 0.3125*raw;
    snprintf(string, size, "\"%f V\"", voltage_mv/1000.0);
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

    if(i == 0) {
        strncpy(string, "nil", size);
        return;
    }
    strncpy(string, "\"", size);
    for(int j = 0; j < i; j++){
        if (j != 0) {
            strncat(string, " | ", size);
        }
        strncat(string, flags[j], size);
    }
    strncat(string, "\"", size);
}

void nRSense_to_str(char * string, size_t size, uint16_t raw) {
    float resistance_mOhm = 0.01 * raw;
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
    {.name = "MaxPeakPower",    .address = 0x00A4, .from_raw = simple_get, .value_2_str = test_parse},
    {.name = "SusPeakPower",    .address = 0x00A5, .from_raw = simple_get, .value_2_str = test_parse},
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
    {.name = "Power",       .address = 0x00B1, .from_raw = simple_get, .value_2_str = test_parse},
    {.name = "VRipple",     .address = 0x00B2, .from_raw = simple_get, .value_2_str = test_parse},
    {.name = "AvgPower",    .address = 0x00B3, .from_raw = simple_get, .value_2_str = test_parse},
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
    char desc_str[512];
    uint16_t value;
    reg->from_raw(reg, &value);
    reg->value_2_str(desc_str, 512, value);
    printf("\"(%03x) %s\": %s", reg->address, reg->name, desc_str);
}

int main(int argc, char* argv[]){
    if (gpioInitialise() < 0) {
        return -1;
    }
    atexit(gpioTerminate);

    if(argc == 1) {
        printf("{\n");
        for(int i = 0; i < sizeof(desc)/sizeof(*desc); i++){
            print_description(&desc[i]);
            if( i != sizeof(desc)/sizeof(*desc) - 1) 
                printf(",\n");
        }
        printf("\n}\n");
        return 0;
    }
    //verify inputs as valid numbers or register names
    for(int i = 1; i < argc; i++){
        char *endptr;
        int addr = strtod(argv[i], &endptr);
        if(addr != 0 || (addr == 0 && endptr != argv[i])){
            int valid_addr = 0;
            for(int j = 0; (j < sizeof(desc)/sizeof(*desc)); j++){
                if(desc[j].address == addr){
                    valid_addr = 1;
                    break;
                }
                if(desc[j].address > addr){
                    break;
                }
            }
            if(!valid_addr){
                printf("Err: register %s is not a valid readable register address\n", argv[i]);
                return -1;
            }
        } else {
            //try name based search
            int vaild_name = 0;
            for(int j = 0; (j < sizeof(desc)/sizeof(*desc)); j++){
                if(strcasecmp(argv[i], desc[j].name) == 0){
                    vaild_name = 1;
                    break;
                }
            }
            if(!vaild_name){
                printf("Err: register \"%s\" is not a valid readable register name\n", argv[i]);
                return -1;
            }
        }
    }


    //print list
    printf("{\n");
    for(int i = 1; i < argc; i++){
        char *endptr;
        int addr = strtod(argv[i], &endptr);
        if(addr != 0 || (addr == 0 && endptr != argv[i])){
            for(int j = 0; (j < sizeof(desc)/sizeof(*desc)); j++){
                if(desc[j].address == addr){
                    print_description(&desc[j]);
                    if( i != argc - 1)
                        printf(",\n");
                    break;
                }
            }
        } else {
            for(int j = 0; (j < sizeof(desc)/sizeof(*desc)); j++){
                if(strcasecmp(argv[i], desc[j].name) == 0){
                    print_description(&desc[j]);
                    if( i != argc - 1)
                        printf(",\n");
                    break;
                }
            }
        }
    }
    printf("\n}\n");
    
    return 0;
}