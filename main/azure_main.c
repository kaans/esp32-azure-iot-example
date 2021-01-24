#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/task.h"
#include <stdio.h>

#include "esp_app_format.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_ota_ops.h"
#include "esp_system.h"
#include "esp_wifi.h"

#include "azure_c_shared_utility/platform.h"
#include "azure_c_shared_utility/threadapi.h"
#include "azure_macro_utils/macro_utils.h"
#include "iothub.h"
#include "iothub_client_options.h"
#include "iothub_device_client.h"
#include "iothub_message.h"
#include "parson.h"

#include "nvs_flash.h"

#include "sdkconfig.h"

#ifdef CONFIG_DEVICE_CONFIG_NVS_FACTORY_ENABLE
#include "device_config.h"
#endif

#ifdef CONFIG_WIFI_PROV_ENABLE
#include "wifi_provisioning.h"
#endif

#ifdef CONFIG_AZURE_IOT_ENABLE
#include "azure_iot_hub.h"
#endif

#ifdef CONFIG_EXAMPLE_GPIO_ENABLE
#include "example_gpio.h"
#endif

#ifdef CONFIG_OTA_UPDATE_HTTPS_ENABLE
#include "ota_update_https.h"
#endif

static const char *TAG = "main";

#ifdef CONFIG_EXAMPLE_DHT_ENABLE
#include "dht.h"

#define CONFIG_EXAMPLE_DHT_THREAD_DELAY 5000

typedef struct dht_result
{
  int16_t humidity;
  int16_t temperature;
} dht_result_t;
#endif

#ifdef CONFIG_AZURE_IOT_ENABLE
static IOTHUBMESSAGE_DISPOSITION_RESULT azure_message_callback(IOTHUB_MESSAGE_HANDLE message, void *user_context)
{
  IOTHUBMESSAGE_CONTENT_TYPE content_type = IoTHubMessage_GetContentType(message);

  if (content_type == IOTHUBMESSAGE_STRING)
  {
    const char *string_value = IoTHubMessage_GetString(message);
    ESP_LOGI(TAG, "String message: %s", string_value);
  }
  else if (content_type == IOTHUBMESSAGE_BYTEARRAY)
  {
    const unsigned char *byte_array_value;
    size_t size;
    IOTHUB_MESSAGE_RESULT result = IoTHubMessage_GetByteArray(message, &byte_array_value, &size);
    if (result == IOTHUB_MESSAGE_OK)
    {
      ESP_LOGI(TAG, "Byte array message: %.*s", size, byte_array_value);
    }
  }
  else
  {
    ESP_LOGI(TAG, "Unknown type message");
  }
  return IOTHUBMESSAGE_ACCEPTED;
}

int azure_method_callback(const char *method_name, const unsigned char *payload, size_t size, unsigned char **response, size_t *response_size, void *userContextCallback)
{
  ESP_LOGI(TAG, "Received command %s with payload %.*s", method_name, size, payload);

  mallocAndStrcpy_s((char **)response, "{ \"success\": \"true\" }");
  *response_size = strlen((char *)*response);

  ESP_LOGI(TAG, "Sending response %.*s", *response_size, *response);

  return IOTHUB_CLIENT_OK;
}

static void reportedStateCallback(int status_code, void *userContextCallback)
{
  (void)userContextCallback;
  printf("Device Twin reported properties update completed with result: %d\r\n", status_code);
}

#ifdef CONFIG_EXAMPLE_DHT_ENABLE
static void send_dht_temperature_reported_state(dht_result_t *dht_result)
{
  char *result;

  JSON_Value *root_value = json_value_init_object();
  JSON_Object *root_object = json_value_get_object(root_value);

  json_object_dotset_number(root_object, "dht.temperature", dht_result->temperature);
  json_object_dotset_number(root_object, "dht.humidity", dht_result->humidity);

  result = json_serialize_to_string(root_value);

  json_value_free(root_value);

  azure_iot_hub_send_reported_state((const unsigned char *)result, strlen(result), reportedStateCallback, NULL);
}
#endif
#endif

#ifdef CONFIG_WIFI_PROV_ENABLE
static void event_handler(void *arg, esp_event_base_t event_base,
                          int32_t event_id, void *event_data)
{
  if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED)
  {
#ifdef CONFIG_AZURE_IOT_ENABLE
    azure_iot_hub_stop();
#endif

#ifdef CONFIG_EXAMPLE_GPIO_ENABLE
    example_gpio_led_set_delay(250);
#endif
  }
  else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP)
  {
#ifdef CONFIG_EXAMPLE_GPIO_ENABLE
    example_gpio_led_set_mode(ON);
#endif

    // call get_time to make sure that the time has been updated
#ifdef CONFIG_OTA_UPDATE_HTTPS_ENABLE
    if (xTaskCreate(&ota_update_https_start_thread, "update_task", 1024 * 5, CONFIG_OTA_UPDATE_HTTPS_URL, 5, NULL) != pdPASS)
    {
      ESP_LOGI(TAG, "create update task failed");
    }
#endif

#ifdef CONFIG_AZURE_IOT_ENABLE
    if (xTaskCreate(&azure_iot_hub_task, "azure_task", 1024 * 5, NULL, 5, NULL) != pdPASS)
    {
      ESP_LOGI(TAG, "create azure task failed");
    }
#endif
  }

#ifndef CONFIG_WIFI_PROV_SCHEME_MANUAL
  else if (event_base == WIFI_PROV_EVENT)
  {
    switch (event_id)
    {
    case WIFI_PROV_START:
      break;
    case WIFI_PROV_CRED_RECV:
#ifdef CONFIG_EXAMPLE_GPIO_ENABLE
      example_gpio_led_set_delay(100);
#endif
      break;
    case WIFI_PROV_CRED_FAIL:
      break;
    case WIFI_PROV_CRED_SUCCESS:
      break;
    case WIFI_PROV_END:
      break;
    default:
      break;
    }
  }
#endif
}

static void initialise_wifi_event_handlers(void)
{
  ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                      ESP_EVENT_ANY_ID,
                                                      &event_handler,
                                                      NULL,
                                                      NULL));
  ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                      IP_EVENT_STA_GOT_IP,
                                                      &event_handler,
                                                      NULL,
                                                      NULL));
#ifndef CONFIG_WIFI_PROV_SCHEME_MANUAL
  ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_PROV_EVENT,
                                                      ESP_EVENT_ANY_ID,
                                                      &event_handler,
                                                      NULL,
                                                      NULL));
#endif
}
#endif

#ifdef CONFIG_EXAMPLE_GPIO_ENABLE

static char system_state[4096];

void button_callback_handler(int button_state)
{
  ESP_LOGI(TAG, "Button state: %i", button_state);
  //azure_iot_hub_send_message("MESSAGE BUTTON PRESSED");
#ifdef CONFIG_FREERTOS_GENERATE_RUN_TIME_STATS
  vTaskGetRunTimeStats(system_state);
#endif
  printf("%s\n", system_state);
}
#endif

static esp_err_t nvs_init_default()
{
  esp_err_t ret = nvs_flash_init();
  if (ret == ESP_ERR_NVS_NO_FREE_PAGES)
  {
    ESP_ERROR_CHECK(nvs_flash_erase());
    ret = nvs_flash_init();
  }
  else if (ret == ESP_ERR_NOT_FOUND)
  {
    ESP_LOGE(TAG, "NVS partition with name nvs was not found. Make sure to include that partition"
                  "in the partition table.");
  }
  return ret;
}

#ifdef CONFIG_EXAMPLE_DHT_ENABLE
static void dht22_thread(void *paramters)
{
  esp_err_t ret;
  dht_result_t result;

  while (true)
  {
    ret = dht_read_data(DHT_TYPE_AM2301, CONFIG_EXAMPLE_DHT_PIN, &(result.humidity), &(result.temperature));

    if (ret == ESP_OK)
    {
      ESP_LOGI(TAG, "Temperature %.2f°C Humidity %.2f %%",
               (float)(result.temperature / 10.0f), (float)(result.humidity / 10.0f));
      send_dht_temperature_reported_state(&result);
    }
    else
    {
      ESP_LOGW(TAG, "Error while reading DHT sensor: %i", ret);
    }

    vTaskDelay(CONFIG_EXAMPLE_DHT_THREAD_DELAY / portTICK_PERIOD_MS);
  }
}
#endif

void app_main()
{
  const esp_app_desc_t *app_description = esp_ota_get_app_description();
  ESP_LOGI(TAG, "App version: %s", app_description->version);
  ESP_LOGI(TAG, "Project name: %s", app_description->project_name);

  // Initialize NVS
  ESP_ERROR_CHECK(nvs_init_default());

  // Initialize default event loop
  ESP_ERROR_CHECK(esp_event_loop_create_default());

#ifdef CONFIG_DEVICE_CONFIG_NVS_FACTORY_ENABLE
  ESP_ERROR_CHECK(device_config_init());
#endif

#ifdef CONFIG_AZURE_IOT_ENABLE
  azure_config_init();
#endif

#ifdef CONFIG_EXAMPLE_DHT_ENABLE
  if (xTaskCreate(&dht22_thread, "dht_task", 1024 * 2, NULL, 7, NULL) != pdPASS)
  {
    ESP_LOGI(TAG, "create dht task failed");
  }
#endif

#ifdef CONFIG_EXAMPLE_GPIO_ENABLE
  example_gpio_led_init(OFF);
  example_gpio_led_set_delay(1500);

  example_gpio_button_set_callback_handler(&button_callback_handler);

  example_gpio_led_blink_task_start();
  example_gpio_button_task_start();
#endif

#ifdef CONFIG_WIFI_PROV_ENABLE
  initialise_wifi_event_handlers();
  wifi_provisioning_start();
#endif

#ifdef CONFIG_AZURE_IOT_ENABLE
  azure_iot_hub_set_message_callback(azure_message_callback, NULL);
  azure_iot_hub_set_device_method_callback(azure_method_callback, NULL);
#endif
}
