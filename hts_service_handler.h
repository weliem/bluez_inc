//
// Created by martijn on 04-11-21.
//

#ifndef TEST_HTS_SERVICE_HANDLER_H
#define TEST_HTS_SERVICE_HANDLER_H

#include "service_handler_manager.h"

#define HTS_SERVICE "00001809-0000-1000-8000-00805f9b34fb"
#define TEMPERATURE_CHAR "00002a1c-0000-1000-8000-00805f9b34fb"

ServiceHandler *hts_service_handler_create();

#endif //TEST_HTS_SERVICE_HANDLER_H
