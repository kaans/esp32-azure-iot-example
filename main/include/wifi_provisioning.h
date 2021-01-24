#ifndef WIFI_PROVISIONING_H
#define WIFI_PROVISIONING_H

#ifndef WIFI_PROV_SCHEME_MANUAL
#include <wifi_provisioning/manager.h>
#endif

void wifi_provisioning_start();

void wifi_provisioning_erase_wifi_config();

#endif //WIFI_PROVISIONING_H
