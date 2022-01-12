//
// Created by martijn on 29-12-21.
//

#ifndef TEST_WSS_SERVICE_HANDLER_H
#define TEST_WSS_SERVICE_HANDLER_H

#include "../service_handler_manager.h"

#define WSS_SERVICE_UUID "0000181d-0000-1000-8000-00805f9b34fb"
#define WSS_MEASUREMENT_CHAR_UUID "00002a9d-0000-1000-8000-00805f9b34fb"
#define WSS_DATE_TIME_CHAR_UUID "00002a08-0000-1000-8000-00805f9b34fb"

ServiceHandler *wss_service_handler_create();


#endif //TEST_WSS_SERVICE_HANDLER_H
