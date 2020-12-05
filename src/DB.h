// Copyright (c) 2015-2019 The HomeKit ADK Contributors
//
// Licensed under the Apache License, Version 2.0 (the “License”);
// you may not use this file except in compliance with the License.
// See [CONTRIBUTORS.md] for the list of HomeKit ADK project authors.

#ifndef DB_H
#define DB_H

#ifdef __cplusplus
extern "C" {
#endif

#include "HAP.h"

#if __has_feature(nullability)
#pragma clang assume_nonnull begin
#endif

/**
 * Services
 */
extern HAPService lightBulbService;
// extern HAPDataCharacteristic lightBulbServiceSignatureCharacteristic;
extern HAPStringCharacteristic lightBulbNameCharacteristic;
extern HAPBoolCharacteristic lightBulbOnCharacteristic;
extern HAPIntCharacteristic lightBulbBrightnessCharacteristic;

#define kIID_PoolSize ((uint64_t) 0x010000)

#if __has_feature(nullability)
#pragma clang assume_nonnull end
#endif

#ifdef __cplusplus
}
#endif

#endif
