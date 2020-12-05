// Copyright (c) 2015-2019 The HomeKit ADK Contributors
//
// Licensed under the Apache License, Version 2.0 (the “License”);
// you may not use this file except in compliance with the License.
// See [CONTRIBUTORS.md] for the list of HomeKit ADK project authors.

#include "App.h"

#include "DB.h"
#include "mgos.h"
#include "mgos_hap.h"
#include "mgos_twinkly.h"
#include "HAPAccessoryServer+Internal.h"

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

/**
 * Domain used in the key value store for application data.
 *
 * Purged: On factory reset.
 */
#define kAppKeyValueStoreDomain_Configuration ((HAPPlatformKeyValueStoreDomain) 0x00)

/**
 * Key used in the key value store to store the configuration state.
 *
 * Purged: On factory reset.
 */
#define kAppKeyValueStoreKey_Configuration_State ((HAPPlatformKeyValueStoreDomain) 0x00)

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool requestedServerRestart = false;

/**
 * Global accessory configuration.
 */

typedef struct {
    bool online;
    bool on;
    int brightness;
} tw_state_t;

typedef struct {
    struct {
        tw_state_t tw_state[MAX_TWINKLY_DEVICES];
    } state;
    HAPAccessoryServerRef* server;
    HAPPlatformKeyValueStoreRef keyValueStore;
} AccessoryConfiguration;

static AccessoryConfiguration accessoryConfiguration;
//----------------------------------------------------------------------------------------------------------------------

/**
 * Load the accessory state from persistent memory.
 */
static void LoadAccessoryState(void) {
    HAPPrecondition(accessoryConfiguration.keyValueStore);

    HAPError err;

    // Load persistent state if available
    bool found;
    size_t numBytes;

    err = HAPPlatformKeyValueStoreGet(
            accessoryConfiguration.keyValueStore,
            kAppKeyValueStoreDomain_Configuration,
            kAppKeyValueStoreKey_Configuration_State,
            &accessoryConfiguration.state,
            sizeof accessoryConfiguration.state,
            &numBytes,
            &found);

    if (err) {
        HAPAssert(err == kHAPError_Unknown);
        HAPFatalError();
    }
    if (!found || numBytes != sizeof accessoryConfiguration.state) {
        if (found) {
            HAPLogError(
                    &kHAPLog_Default,
                    "Unexpected app state found in key-value store. Resetting to "
                    "default.");
        }
        HAPRawBufferZero(&accessoryConfiguration.state, sizeof accessoryConfiguration.state);
    }
}

/**
 * Save the accessory state to persistent memory.
 */
static void SaveAccessoryState(void) {
    HAPPrecondition(accessoryConfiguration.keyValueStore);

    HAPError err;
    err = HAPPlatformKeyValueStoreSet(
            accessoryConfiguration.keyValueStore,
            kAppKeyValueStoreDomain_Configuration,
            kAppKeyValueStoreKey_Configuration_State,
            &accessoryConfiguration.state,
            sizeof accessoryConfiguration.state);
    if (err) {
        HAPAssert(err == kHAPError_Unknown);
        HAPFatalError();
    }
}

//----------------------------------------------------------------------------------------------------------------------

/**
 * HomeKit accessory that provides the Light Bulb service.
 *
 * Note: Not constant to enable BCT Manual Name Change.
 */
static HAPAccessory accessory = { .aid = 1,
                                  .category = kHAPAccessoryCategory_Lighting,
                                  .name = CS_STRINGIFY_MACRO(HAP_PRODUCT_NAME),
                                  .manufacturer = CS_STRINGIFY_MACRO(HAP_PRODUCT_VENDOR),
                                  .model = CS_STRINGIFY_MACRO(HAP_PRODUCT_MODEL),
                                  .serialNumber = NULL,    // Set from config.
                                  .firmwareVersion = NULL, // Set from build_id.
                                  .hardwareVersion = CS_STRINGIFY_MACRO(HAP_PRODUCT_HW_REV),
                                  .services = NULL, // dynamic loading
                                  //   (const HAPService* const[]) { &mgos_hap_accessory_information_service,
                                  //                                 &mgos_hap_protocol_information_service,
                                  //                                 &mgos_hap_pairing_service,
                                  //                                 &servie1, service2,...
                                  //                                 NULL },
                                  .callbacks = { .identify = IdentifyAccessory } };

//----------------------------------------------------------------------------------------------------------------------

void AccessoryNotification(const HAPService* service, const HAPCharacteristic* characteristic) {
    HAPLogInfo(&kHAPLog_Default, "Accessory Notification");

    HAPAccessoryServerRaiseEvent(accessoryConfiguration.server, characteristic, service, &accessory);
}

static void identify_timer_cb(void* arg) {
    mgos_gpio_blink(mgos_sys_config_get_pins_led(), 0, 0);
    mgos_gpio_write(mgos_sys_config_get_pins_led(), LED_OFF);
    (void) arg;
}

HAP_RESULT_USE_CHECK
HAPError IdentifyAccessory(
        HAPAccessoryServerRef* server HAP_UNUSED,
        const HAPAccessoryIdentifyRequest* request HAP_UNUSED,
        void* _Nullable context HAP_UNUSED) {
    HAPLogInfo(&kHAPLog_Default, "%s", __func__);
    mgos_gpio_blink(mgos_sys_config_get_pins_led(), 50, 100);
    mgos_set_timer(1000, 0, identify_timer_cb, NULL);
    return kHAPError_None;
}

HAP_RESULT_USE_CHECK
HAPError HandleLightBulbOnRead(
        HAPAccessoryServerRef* server HAP_UNUSED,
        const HAPBoolCharacteristicReadRequest* request HAP_UNUSED,
        bool* value,
        void* _Nullable context HAP_UNUSED) {
    int index = request->characteristic->iid >> 16;
    *value = accessoryConfiguration.state.tw_state[index].on;
    HAPLogInfo(&kHAPLog_Default, "%s: %s", __func__, *value ? "true" : "false");

    return kHAPError_None;
}

HAP_RESULT_USE_CHECK
HAPError HandleLightBulbOnWrite(
        HAPAccessoryServerRef* server,
        const HAPBoolCharacteristicWriteRequest* request,
        bool value,
        void* _Nullable context HAP_UNUSED) {
    HAPLogInfo(&kHAPLog_Default, "%s: %s", __func__, value ? "true" : "false");
    int index = request->characteristic->iid >> 16;
    if (accessoryConfiguration.state.tw_state[index].on != value) {
        accessoryConfiguration.state.tw_state[index].on = value;

        mgos_twinkly_set_mode(index, value);

        SaveAccessoryState();

        HAPAccessoryServerRaiseEvent(server, request->characteristic, request->service, request->accessory);
    }

    return kHAPError_None;
}

HAP_RESULT_USE_CHECK
HAPError HandleLightBulbBrightnessRead(
        HAPAccessoryServerRef* server HAP_UNUSED,
        const HAPIntCharacteristicReadRequest* request HAP_UNUSED,
        int* value,
        void* _Nullable context HAP_UNUSED) {
    int index = request->characteristic->iid >> 16;
    *value = accessoryConfiguration.state.tw_state[index].brightness;
    HAPLogInfo(&kHAPLog_Default, "%s: %ld", __func__, (long) *value);

    return kHAPError_None;
}

HAP_RESULT_USE_CHECK
HAPError HandleLightBulbBrightnessWrite(
        HAPAccessoryServerRef* server,
        const HAPIntCharacteristicWriteRequest* request,
        int value,
        void* _Nullable context HAP_UNUSED) {
    HAPLogInfo(&kHAPLog_Default, "%s: %ld", __func__, (long) value);
    int index = request->characteristic->iid >> 16;

    if (accessoryConfiguration.state.tw_state[index].brightness != value) {
        accessoryConfiguration.state.tw_state[index].brightness = value;

        mgos_twinkly_set_brightness(index, value);

        SaveAccessoryState();

        HAPAccessoryServerRaiseEvent(server, request->characteristic, request->service, request->accessory);
    }

    return kHAPError_None;
}

//----------------------------------------------------------------------------------------------------------------------

void AppCreate(HAPAccessoryServerRef* server, HAPPlatformKeyValueStoreRef keyValueStore) {
    HAPPrecondition(server);
    HAPPrecondition(keyValueStore);

    HAPLogInfo(&kHAPLog_Default, "%s", __func__);

    HAPRawBufferZero(&accessoryConfiguration, sizeof accessoryConfiguration);
    accessoryConfiguration.server = server;
    accessoryConfiguration.keyValueStore = keyValueStore;
    LoadAccessoryState();
}

void AppRelease(void) {
}

bool HAPServiceCreate_cb(int idx, const struct mg_str* ip, const struct mg_str* json) {
    LOG(LL_DEBUG, ("%s %.*s %.*s", __func__, ip->len, ip->p, json->len, json->p));
    if (idx >= MAX_TWINKLY_DEVICES) {
        LOG(LL_ERROR, ("%s max devices reached %d", __func__, MAX_TWINKLY_DEVICES));
        return false;
    }
    HAPService* service = calloc(1, sizeof(HAPService));
    HAPService** index = (void*) &accessory.services[idx + 3]; // acc_info + prot_info + pairing
    *index++ = service;
    *index = NULL; // NULL terminated always
    memcpy(service, &lightBulbService, sizeof(HAPService));
    service->iid += kIID_PoolSize * idx;
    LOG(LL_DEBUG, ("%s s->iid %lld", __func__, service->iid));
    HAPCharacteristic** cs = calloc(4 + 1, sizeof(HAPCharacteristic*)); // 4 chars + NULL
    service->characteristics = (const HAPCharacteristic* const*) cs;
    // service->properties.primaryService = (idx == 0);
    // Lightbulb name
    char* name = NULL;
    if (json_scanf(json->p, json->len, "{device_name: %Q}", &name) == 1) {
        service->name = strdup(name);
        LOG(LL_INFO, ("Twinkly %ld: %s", (long) idx, service->name));
    } else
        LOG(LL_INFO, ("Twinkly %ld: no name, using accessory name", (long) idx));
    // Need this? i don't think so
    // HAPDataCharacteristic* SignatureCharacteristic = calloc(1, sizeof(HAPDataCharacteristic));
    // memcpy(SignatureCharacteristic, &lightBulbServiceSignatureCharacteristic, sizeof(HAPDataCharacteristic));
    // SignatureCharacteristic->iid += kIID_PoolSize * idx;
    // *cs++ = SignatureCharacteristic;
    //
    HAPStringCharacteristic* NameCharacteristic = calloc(1, sizeof(HAPStringCharacteristic));
    memcpy(NameCharacteristic, &lightBulbNameCharacteristic, sizeof(HAPStringCharacteristic));
    NameCharacteristic->iid += kIID_PoolSize * idx;
    *cs++ = NameCharacteristic;
    //
    HAPBoolCharacteristic* OnCharacteristic = calloc(1, sizeof(HAPBoolCharacteristic));
    memcpy(OnCharacteristic, &lightBulbOnCharacteristic, sizeof(HAPBoolCharacteristic));
    OnCharacteristic->iid += kIID_PoolSize * idx;
    *cs++ = OnCharacteristic;
    //
    HAPIntCharacteristic* BrightnessCharacteristic = calloc(1, sizeof(HAPIntCharacteristic));
    memcpy(BrightnessCharacteristic, &lightBulbBrightnessCharacteristic, sizeof(HAPIntCharacteristic));
    BrightnessCharacteristic->iid += kIID_PoolSize * idx;
    *cs++ = BrightnessCharacteristic;
    //
    *cs = NULL;
    return true;
}

void RestartHAPServer() {
    if (HAPAccessoryServerGetState(accessoryConfiguration.server) == kHAPAccessoryServerState_Running) {
        HAPAccessoryServerStop(accessoryConfiguration.server);
    }
}

void AppAccessoryServerStart(void) {
    LOG(LL_DEBUG, (__func__));
    int n = mgos_twinkly_count();
    if (!n) {
        LOG(LL_ERROR, ("No devices to expose. Add first"));
        return;
    }
    // Cleaning up services
    if (accessory.services) {
        int i = 3; // skipping constant services
        while (accessory.services[i]) {
            int j = 0;
            while (accessory.services[i]->characteristics[j]) {
                free((void*) accessory.services[i]->characteristics[j]);
                j++;
            };
            free((void*) accessory.services[i]->name);
            free((void*) accessory.services[i]);
            i++;
        };
        free((void*) accessory.services);
    }
    // Loading services
    accessory.services = calloc(3 + n + 1, sizeof(HAPService*));
    memset((void*) accessory.services, 0, (3 + n + 1) * sizeof(HAPService*)); // NULL terminated
    HAPService** index = (void*) &accessory.services[0];
    *index++ = (HAPService*) &mgos_hap_accessory_information_service;
    *index++ = (HAPService*) &mgos_hap_protocol_information_service;
    *index++ = (HAPService*) &mgos_hap_pairing_service;
    mgos_twinkly_iterate(HAPServiceCreate_cb);
    LOG(LL_INFO, ("Twinkly devices loaded: %ld", (long) n));
    // Shifting CN to reload accessory if devices were added/removed
    if (mgos_sys_config_get_twinkly_config_changed()) {
        LOG(LL_INFO, ("Twinkly configuration changed, increasing CN"));
        HAPError err = HAPAccessoryServerIncrementCN(accessoryConfiguration.keyValueStore);
        if (err) {
            LOG(LL_ERROR, ("Failed to increase CN"));
        } else {
            mgos_sys_config_set_twinkly_config_changed(false);
            mgos_sys_config_save(&mgos_sys_config, false, NULL);
        }
    }
    HAPAccessoryServerStart(accessoryConfiguration.server, &accessory);
}

//----------------------------------------------------------------------------------------------------------------------

void AccessoryServerHandleUpdatedState(HAPAccessoryServerRef* server, void* _Nullable context) {
    HAPPrecondition(server);
    HAPPrecondition(!context);

    switch (HAPAccessoryServerGetState(server)) {
        case kHAPAccessoryServerState_Idle: {
            HAPLogInfo(&kHAPLog_Default, "Accessory Server State did update: Idle.");
            return;
        }
        case kHAPAccessoryServerState_Running: {
            HAPLogInfo(&kHAPLog_Default, "Accessory Server State did update: Running.");
            return;
        }
        case kHAPAccessoryServerState_Stopping: {
            HAPLogInfo(&kHAPLog_Default, "Accessory Server State did update: Stopping.");
            return;
        }
    }
    HAPFatalError();
}

HAPAccessory* AppGetAccessoryInfo() {
    return &accessory;
}

void AppInitialize(
        HAPAccessoryServerOptions* hapAccessoryServerOptions HAP_UNUSED,
        HAPPlatform* hapPlatform HAP_UNUSED,
        HAPAccessoryServerCallbacks* hapAccessoryServerCallbacks HAP_UNUSED) {
    accessory.firmwareVersion = mgos_sys_ro_vars_get_fw_version();
    accessory.serialNumber = mgos_sys_config_get_device_id();
    static char hostname[13] = "TWH-????";
    mgos_expand_mac_address_placeholders(hostname);
    accessory.name = hostname;
}

void AppDeinitialize() {
    /*no-op*/
}

static void led_off_timer_cb(void* arg) {
    mgos_gpio_write(mgos_sys_config_get_pins_led(), LED_OFF);
}

static void led_on(int msec) {
    mgos_gpio_write(mgos_sys_config_get_pins_led(), LED_ON);
    mgos_set_timer(msec, 0, led_off_timer_cb, NULL);
}

void twinkly_cb(int ev, void* ev_data, void* userdata) {
    mgos_twinkly_ev_data_t* data = ev_data;
    switch (ev) {
        case MGOS_TWINKLY_EV_INITIALIZED: {
            LOG(LL_DEBUG, ("Twinkly init done"));
            led_on(250);
        } break;
        case MGOS_TWINKLY_EV_STATUS: {
            int status = data->value;
            LOG(LL_INFO, ("Twinkly %ld status: %s", (long) data->index, status ? "online" : "offline"));
            if (data->index >= MAX_TWINKLY_DEVICES)
                break;
            accessoryConfiguration.state.tw_state[data->index].online = (bool) status;
            // #todo Raise StstusActive event
            // if (HAPAccessoryServerGetState(accessoryConfiguration.server) == kHAPAccessoryServerState_Running)
            // AccessoryNotification(accessory.services[3+data->index],accessory.services[3+data->index]->characteristics[3]);
            led_on(150);
        } break;
        case MGOS_TWINKLY_EV_MODE: {
            int mode = data->value;
            LOG(LL_INFO, ("Twinkly %ld mode: %s", (long) data->index, mode ? "on" : "off"));
            if (data->index >= MAX_TWINKLY_DEVICES)
                break;
            accessoryConfiguration.state.tw_state[data->index].on = (bool) mode;
            if (HAPAccessoryServerGetState(accessoryConfiguration.server) == kHAPAccessoryServerState_Running)
                AccessoryNotification(
                        accessory.services[3 + data->index], accessory.services[3 + data->index]->characteristics[1]);
            led_on(150);
        } break;
        case MGOS_TWINKLY_EV_BRIGHTNESS: {
            int brightness = data->value;
            LOG(LL_INFO, ("Twinkly %ld brightness: %ld", (long) data->index, (long) brightness));
            if (data->index >= MAX_TWINKLY_DEVICES)
                break;
            accessoryConfiguration.state.tw_state[data->index].brightness = brightness;
            if (HAPAccessoryServerGetState(accessoryConfiguration.server) == kHAPAccessoryServerState_Running)
                AccessoryNotification(
                        accessory.services[3 + data->index], accessory.services[3 + data->index]->characteristics[2]);
            led_on(150);
        } break;
        case MGOS_TWINKLY_EV_ADDED:
        case MGOS_TWINKLY_EV_REMOVED: {
            LOG(LL_INFO, ("Twinkly device list changed, restarting HAP server"));
            led_on(400);
            RestartHAPServer();
            requestedServerRestart = true;
        } break;
        default:
            LOG(LL_VERBOSE_DEBUG, ("event: %d", ev));
    }
}
