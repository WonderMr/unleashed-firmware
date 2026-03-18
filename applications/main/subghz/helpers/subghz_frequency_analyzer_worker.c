#include "subghz_frequency_analyzer_worker.h"
#include <cc1101_regs.h>

#include <furi.h>
#include <float_tools.h>
#include <lib/subghz/devices/devices.h>

#define TAG "SubghzFrequencyAnalyzerWorker"

#define SUBGHZ_FREQUENCY_ANALYZER_THRESHOLD -97.0f

// Full custom preset for coarse scan (650kHz BW).
// Based on subghz_device_cc1101_preset_ook_650khz_async_regs but with:
//   - IOCFG0 = CC1101IocfgHW (not async data)
//   - MDMCFG3 = 0x7F (higher symbol rate for faster RSSI)
//   - Custom AGC settings optimized for signal detection
static const uint8_t subghz_frequency_analyzer_preset_650khz[] = {
    // GPIO GD0
    CC1101_IOCFG0,
    CC1101IocfgHW,

    // FIFO and internals
    CC1101_FIFOTHR,
    0x07, // ADC_RETENTION

    // Packet engine
    CC1101_PKTCTRL0,
    0x32, // Async, continuous, no whitening

    // Frequency Synthesizer Control
    CC1101_FSCTRL1,
    0x06, // IF = 152343.75Hz

    // Modem Configuration
    CC1101_MDMCFG0,
    0x00, // Channel spacing is 25kHz
    CC1101_MDMCFG1,
    0x00, // Channel spacing is 25kHz
    CC1101_MDMCFG2,
    0x30, // Format ASK/OOK, No preamble/sync
    CC1101_MDMCFG3,
    0b01111111, // Symbol rate
    CC1101_MDMCFG4,
    0b00010111, // Rx BW filter is 650.000kHz

    // Main Radio Control State Machine
    CC1101_MCSM0,
    0b00011000, // FS_AUTOCAL=01: auto-calibrate on IDLE->RX/TX; PO_TIMEOUT=10

    // Frequency Offset Compensation Configuration
    CC1101_FOCCFG,
    0x18,

    // Automatic Gain Control (custom for analyzer)
    CC1101_AGCCTRL0,
    0b00110000, // No hysteresis, 64 samples AGC, Normal AGC, 4dB boundary
    CC1101_AGCCTRL1,
    0b00001000, // LNA2 decreased first, carrier sense threshold disabled
    CC1101_AGCCTRL2,
    0b00000111, // DVGA all, MAX LNA+LNA2, MAGN_TARGET 42 dB

    // Wake on radio and timeouts control
    CC1101_WORCTRL,
    0xFB,

    // Frontend configuration
    CC1101_FREND0,
    0x11,
    CC1101_FREND1,
    0xB6,

    // End of register config
    0,
    0,

    // PA table (8 bytes)
    0x00,
    0xC0,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
};

// Full custom preset for fine scan (58kHz BW).
// Same as coarse but with narrow bandwidth for precise frequency determination.
static const uint8_t subghz_frequency_analyzer_preset_58khz[] = {
    // GPIO GD0
    CC1101_IOCFG0,
    CC1101IocfgHW,

    // FIFO and internals
    CC1101_FIFOTHR,
    0x07,

    // Packet engine
    CC1101_PKTCTRL0,
    0x32,

    // Frequency Synthesizer Control
    CC1101_FSCTRL1,
    0x06,

    // Modem Configuration
    CC1101_MDMCFG0,
    0x00,
    CC1101_MDMCFG1,
    0x00,
    CC1101_MDMCFG2,
    0x30,
    CC1101_MDMCFG3,
    0b01111111,
    CC1101_MDMCFG4,
    0b11110111, // Rx BW filter is 58.035714kHz

    // Main Radio Control State Machine
    CC1101_MCSM0,
    0b00011000,

    // Frequency Offset Compensation Configuration
    CC1101_FOCCFG,
    0x18,

    // Automatic Gain Control (same as coarse)
    CC1101_AGCCTRL0,
    0b00110000,
    CC1101_AGCCTRL1,
    0b00001000,
    CC1101_AGCCTRL2,
    0b00000111,

    // Wake on radio and timeouts control
    CC1101_WORCTRL,
    0xFB,

    // Frontend configuration
    CC1101_FREND0,
    0x11,
    CC1101_FREND1,
    0xB6,

    // End of register config
    0,
    0,

    // PA table (8 bytes)
    0x00,
    0xC0,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
};

struct SubGhzFrequencyAnalyzerWorker {
    FuriThread* thread;

    volatile bool worker_running;
    uint8_t sample_hold_counter;
    FrequencyRSSI frequency_rssi_buf;
    SubGhzSetting* setting;
    const SubGhzDevice* radio_device;

    float filVal;
    float trigger_level;

    SubGhzFrequencyAnalyzerWorkerPairCallback pair_callback;
    void* context;
};

// running average with adaptive coefficient
static uint32_t subghz_frequency_analyzer_worker_expRunningAverageAdaptive(
    SubGhzFrequencyAnalyzerWorker* instance,
    uint32_t newVal) {
    float k;
    float newValFloat = newVal;
    // the sharpness of the filter depends on the absolute value of the difference
    if(fabsf(newValFloat - instance->filVal) > 500000.f)
        k = 0.9;
    else
        k = 0.03;

    instance->filVal += (newValFloat - instance->filVal) * k;
    return (uint32_t)instance->filVal;
}

/** Worker thread */
static int32_t subghz_frequency_analyzer_worker_thread(void* context) {
    SubGhzFrequencyAnalyzerWorker* instance = context;

    FrequencyRSSI frequency_rssi = {
        .frequency_coarse = 0, .rssi_coarse = 0, .frequency_fine = 0, .rssi_fine = 0};
    float rssi = 0;
    uint32_t frequency = 0;
    float rssi_temp = 0;
    uint32_t frequency_temp = 0;

    const SubGhzDevice* device = instance->radio_device;

    // Initialize radio with coarse scan preset
    subghz_devices_reset(device);
    subghz_devices_load_preset(
        device, FuriHalSubGhzPresetCustom, (uint8_t*)subghz_frequency_analyzer_preset_650khz);

    while(instance->worker_running) {
        furi_delay_ms(10);

        float rssi_min = 26.0f;
        float rssi_avg = 0;
        size_t rssi_avg_samples = 0;

        frequency_rssi.rssi_coarse = -127.0f;
        frequency_rssi.rssi_fine = -127.0f;

        // Load coarse scan preset (650kHz BW)
        subghz_devices_idle(device);
        subghz_devices_load_preset(
            device, FuriHalSubGhzPresetCustom, (uint8_t*)subghz_frequency_analyzer_preset_650khz);

        // First stage: coarse scan
        for(size_t i = 0; i < subghz_setting_get_frequency_count(instance->setting); i++) {
            if(!instance->worker_running) break;

            uint32_t current_frequency = subghz_setting_get_frequency(instance->setting, i);
            if(subghz_devices_is_frequency_valid(device, current_frequency) &&
               (((current_frequency != 462750000) && (current_frequency != 467750000) &&
                 (current_frequency != 464000000)) &&
                (current_frequency <= 920000000))) {
                // set_frequency handles RF path switching internally
                subghz_devices_idle(device);
                frequency = subghz_devices_set_frequency(device, current_frequency);
                // set_rx waits for RX state (including auto-calibration)
                subghz_devices_set_rx(device);

                furi_delay_us(800); // RSSI settling for 650kHz BW

                rssi = subghz_devices_get_rssi(device);

                rssi_avg += rssi;
                rssi_avg_samples++;

                if(rssi < rssi_min) rssi_min = rssi;

                if(frequency_rssi.rssi_coarse < rssi) {
                    frequency_rssi.rssi_coarse = rssi;
                    frequency_rssi.frequency_coarse = frequency;
                }
            }
        }

        if(!instance->worker_running) break;

        FURI_LOG_T(
            TAG,
            "RSSI: avg %f, max %f at %lu, min %f",
            (double)(rssi_avg / rssi_avg_samples),
            (double)frequency_rssi.rssi_coarse,
            frequency_rssi.frequency_coarse,
            (double)rssi_min);

        // Second stage: fine scan
        if(frequency_rssi.rssi_coarse > instance->trigger_level) {
            // Load fine scan preset (58kHz BW)
            subghz_devices_idle(device);
            subghz_devices_load_preset(
                device,
                FuriHalSubGhzPresetCustom,
                (uint8_t*)subghz_frequency_analyzer_preset_58khz);

            //for example -0.3 ... 433.92 ... +0.3 step 20KHz
            for(uint32_t i = frequency_rssi.frequency_coarse - 300000;
                i < frequency_rssi.frequency_coarse + 300000;
                i += 20000) {
                if(!instance->worker_running) break;

                if(subghz_devices_is_frequency_valid(device, i)) {
                    subghz_devices_idle(device);
                    frequency = subghz_devices_set_frequency(device, i);
                    subghz_devices_set_rx(device);

                    furi_delay_ms(2); // Fine scan needs longer settling (58kHz BW)

                    rssi = subghz_devices_get_rssi(device);

                    FURI_LOG_T(TAG, "#:%lu:%f", frequency, (double)rssi);

                    if(frequency_rssi.rssi_fine < rssi) {
                        frequency_rssi.rssi_fine = rssi;
                        frequency_rssi.frequency_fine = frequency;
                    }
                }
            }
        }

        // Deliver results fine
        if(frequency_rssi.rssi_fine > instance->trigger_level) {
            FURI_LOG_D(
                TAG, "=:%lu:%f", frequency_rssi.frequency_fine, (double)frequency_rssi.rssi_fine);

            instance->sample_hold_counter = 20;
            rssi_temp = frequency_rssi.rssi_fine;
            frequency_temp = frequency_rssi.frequency_fine;

            if(!float_is_equal(instance->filVal, 0.f)) {
                frequency_rssi.frequency_fine =
                    subghz_frequency_analyzer_worker_expRunningAverageAdaptive(
                        instance, frequency_rssi.frequency_fine);
            }
            // Deliver callback
            if(instance->pair_callback) {
                instance->pair_callback(
                    instance->context,
                    frequency_rssi.frequency_fine,
                    frequency_rssi.rssi_fine,
                    true);
            }
        } else if( // Deliver results coarse
            (frequency_rssi.rssi_coarse > instance->trigger_level) &&
            (instance->sample_hold_counter < 10)) {
            FURI_LOG_D(
                TAG,
                "~:%lu:%f",
                frequency_rssi.frequency_coarse,
                (double)frequency_rssi.rssi_coarse);

            instance->sample_hold_counter = 20;
            rssi_temp = frequency_rssi.rssi_coarse;
            frequency_temp = frequency_rssi.frequency_coarse;
            if(!float_is_equal(instance->filVal, 0.f)) {
                frequency_rssi.frequency_coarse =
                    subghz_frequency_analyzer_worker_expRunningAverageAdaptive(
                        instance, frequency_rssi.frequency_coarse);
            }
            // Deliver callback
            if(instance->pair_callback) {
                instance->pair_callback(
                    instance->context,
                    frequency_rssi.frequency_coarse,
                    frequency_rssi.rssi_coarse,
                    true);
            }
        } else {
            if(instance->sample_hold_counter > 0) {
                instance->sample_hold_counter--;
                if(instance->sample_hold_counter == 18) {
                    if(instance->pair_callback) {
                        instance->pair_callback(
                            instance->context, frequency_temp, rssi_temp, false);
                    }
                }
            } else {
                instance->filVal = 0;
                if(instance->pair_callback)
                    instance->pair_callback(instance->context, 0, 0, false);
            }
        }
    }

    //Stop radio
    subghz_devices_idle(device);
    subghz_devices_sleep(device);

    return 0;
}

SubGhzFrequencyAnalyzerWorker* subghz_frequency_analyzer_worker_alloc(void* context) {
    furi_assert(context);
    SubGhzFrequencyAnalyzerWorker* instance = malloc(sizeof(SubGhzFrequencyAnalyzerWorker));

    instance->thread = furi_thread_alloc_ex(
        "SubGhzFAWorker", 2048, subghz_frequency_analyzer_worker_thread, instance);
    SubGhz* subghz = context;
    instance->setting = subghz_txrx_get_setting(subghz->txrx);
    instance->radio_device = subghz_txrx_get_radio_device(subghz->txrx);
    instance->trigger_level = subghz->last_settings->frequency_analyzer_trigger;
    return instance;
}

void subghz_frequency_analyzer_worker_free(SubGhzFrequencyAnalyzerWorker* instance) {
    furi_assert(instance);

    furi_thread_free(instance->thread);
    free(instance);
}

void subghz_frequency_analyzer_worker_set_pair_callback(
    SubGhzFrequencyAnalyzerWorker* instance,
    SubGhzFrequencyAnalyzerWorkerPairCallback callback,
    void* context) {
    furi_assert(instance);
    furi_assert(context);
    instance->pair_callback = callback;
    instance->context = context;
}

void subghz_frequency_analyzer_worker_start(SubGhzFrequencyAnalyzerWorker* instance) {
    furi_assert(instance);
    furi_assert(!instance->worker_running);

    instance->worker_running = true;

    furi_thread_start(instance->thread);
}

void subghz_frequency_analyzer_worker_stop(SubGhzFrequencyAnalyzerWorker* instance) {
    furi_assert(instance);
    furi_assert(instance->worker_running);

    instance->worker_running = false;

    furi_thread_join(instance->thread);
}

bool subghz_frequency_analyzer_worker_is_running(SubGhzFrequencyAnalyzerWorker* instance) {
    furi_assert(instance);
    return instance->worker_running;
}

void subghz_frequency_analyzer_worker_set_trigger_level(
    SubGhzFrequencyAnalyzerWorker* instance,
    float value) {
    instance->trigger_level = value;
}

float subghz_frequency_analyzer_worker_get_trigger_level(SubGhzFrequencyAnalyzerWorker* instance) {
    return instance->trigger_level;
}

uint32_t subghz_frequency_analyzer_get_nearest_frequency(
    SubGhzFrequencyAnalyzerWorker* instance,
    uint32_t input) {
    uint32_t result = 0;
    uint32_t best_diff = UINT32_MAX;

    for(size_t i = 0; i < subghz_setting_get_frequency_count(instance->setting); i++) {
        uint32_t current = subghz_setting_get_frequency(instance->setting, i);
        if(current == 0) {
            continue;
        }
        uint32_t diff = (current > input) ? (current - input) : (input - current);
        if(diff < best_diff) {
            best_diff = diff;
            result = current;
            if(diff == 0) break; // Exact match
        }
    }

    return result;
}
