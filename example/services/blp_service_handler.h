//
// Created by martijn on 07-11-21.
//

#ifndef TEST_BLP_SERVICE_HANDLER_H
#define TEST_BLP_SERVICE_HANDLER_H

#include "../service_handler_manager.h"

#define BLP_SERVICE_UUID "00001810-0000-1000-8000-00805f9b34fb"
#define BLOODPRESSURE_CHAR_UUID "00002a35-0000-1000-8000-00805f9b34fb"

ServiceHandler *blp_service_handler_create();

#endif //TEST_BLP_SERVICE_HANDLER_H
