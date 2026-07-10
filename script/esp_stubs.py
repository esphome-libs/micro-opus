"""ESP-IDF stub headers for the include checker.

Minimal stand-ins for the ESP-IDF headers this library family uses, so
check_includes.py can analyze #ifdef ESP_PLATFORM branches with host clang
tooling. No ESP-IDF checkout or cross toolchain is required.

Each entry is named exactly like the real header and declares only the
symbols the micro-* / sendspin family actually references. Because the file
names match, include-what-you-use attribution resolves correctly: code using
heap_caps_malloc is told to include <esp_heap_caps.h>, which is also the
right answer against real ESP-IDF.

Each run materializes these headers into the tool-owned build directory
(build/check_includes/esp_stubs/); this file is the only artifact checked
in. Signatures are simplified where the real header uses macros or
config-dependent types, which is fine for include analysis: it only needs
to map symbol names to headers.

If the checker reports a compile error about a missing ESP-IDF symbol, add
the smallest possible declaration to the matching entry here, or a new
entry if the header is new. Keep this file identical across the family
repos so it can be copied wholesale.
"""

# Mapping: header path (as included) -> file content.
STUBS = {
    "esp_err.h": r'''// Stub of ESP-IDF esp_err.h, materialized by check-includes from esp_stubs.py.
#pragma once

typedef int esp_err_t;

#define ESP_OK 0
#define ESP_FAIL (-1)
#define ESP_ERR_NO_MEM 0x101
#define ESP_ERR_INVALID_ARG 0x102
#define ESP_ERR_INVALID_STATE 0x103
#define ESP_ERR_INVALID_SIZE 0x104
#define ESP_ERR_NOT_FOUND 0x105
#define ESP_ERR_NOT_SUPPORTED 0x106
#define ESP_ERR_TIMEOUT 0x107

#ifdef __cplusplus
extern "C" {
#endif

const char *esp_err_to_name(esp_err_t code);
void _esp_error_check_failed(esp_err_t rc, const char *file, int line, const char *function,
                             const char *expression);

#ifdef __cplusplus
}
#endif

#define ESP_ERROR_CHECK(x)                                                    \
    do {                                                                      \
        esp_err_t err_rc_ = (x);                                              \
        if (err_rc_ != ESP_OK) {                                              \
            _esp_error_check_failed(err_rc_, __FILE__, __LINE__, __func__, #x); \
        }                                                                     \
    } while (0)
''',
    "esp_heap_caps.h": r'''// Stub of ESP-IDF esp_heap_caps.h, materialized by check-includes from esp_stubs.py.
#pragma once

#include <stddef.h>
#include <stdint.h>

#define MALLOC_CAP_EXEC (1 << 0)
#define MALLOC_CAP_32BIT (1 << 1)
#define MALLOC_CAP_8BIT (1 << 2)
#define MALLOC_CAP_DMA (1 << 3)
#define MALLOC_CAP_SPIRAM (1 << 10)
#define MALLOC_CAP_INTERNAL (1 << 11)
#define MALLOC_CAP_DEFAULT (1 << 12)

#ifdef __cplusplus
extern "C" {
#endif

void *heap_caps_malloc(size_t size, uint32_t caps);
void *heap_caps_calloc(size_t n, size_t size, uint32_t caps);
void *heap_caps_realloc(void *ptr, size_t size, uint32_t caps);
void *heap_caps_aligned_alloc(size_t alignment, size_t size, uint32_t caps);
void *heap_caps_malloc_prefer(size_t size, size_t num, ...);
void *heap_caps_calloc_prefer(size_t n, size_t size, size_t num, ...);
void *heap_caps_realloc_prefer(void *ptr, size_t size, size_t num, ...);
void heap_caps_free(void *ptr);
size_t heap_caps_get_free_size(uint32_t caps);
size_t heap_caps_get_minimum_free_size(uint32_t caps);
size_t heap_caps_get_largest_free_block(uint32_t caps);

#ifdef __cplusplus
}
#endif
''',
    "esp_http_server.h": r'''// Stub of ESP-IDF esp_http_server.h, materialized by check-includes from esp_stubs.py.
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef void *httpd_handle_t;

#define ESP_HTTPD_DEF_CTRL_PORT 32768

typedef enum {
    HTTP_GET = 1,
    HTTP_POST = 3,
} httpd_method_t;

typedef enum {
    HTTPD_WS_TYPE_CONTINUE = 0x0,
    HTTPD_WS_TYPE_TEXT = 0x1,
    HTTPD_WS_TYPE_BINARY = 0x2,
    HTTPD_WS_TYPE_CLOSE = 0x8,
    HTTPD_WS_TYPE_PING = 0x9,
    HTTPD_WS_TYPE_PONG = 0xA,
} httpd_ws_type_t;

typedef struct {
    bool final;
    bool fragmented;
    httpd_ws_type_t type;
    uint8_t *payload;
    size_t len;
} httpd_ws_frame_t;

typedef struct httpd_req {
    httpd_handle_t handle;
    int method;
    void *user_ctx;
    void *sess_ctx;
} httpd_req_t;

typedef esp_err_t (*httpd_uri_handler_t)(httpd_req_t *req);

typedef struct {
    const char *uri;
    httpd_method_t method;
    httpd_uri_handler_t handler;
    void *user_ctx;
    bool is_websocket;
    bool handle_ws_control_frames;
    const char *supported_subprotocol;
} httpd_uri_t;

typedef esp_err_t (*httpd_open_func_t)(httpd_handle_t hd, int sockfd);
typedef void (*httpd_close_func_t)(httpd_handle_t hd, int sockfd);
typedef void (*httpd_free_ctx_fn_t)(void *ctx);
typedef void (*httpd_work_fn_t)(void *arg);

typedef struct {
    unsigned task_priority;
    size_t stack_size;
    int core_id;
    uint32_t task_caps;
    uint16_t server_port;
    uint16_t ctrl_port;
    uint16_t max_open_sockets;
    uint16_t max_uri_handlers;
    uint16_t max_resp_headers;
    uint16_t backlog_conn;
    bool lru_purge_enable;
    uint16_t recv_wait_timeout;
    uint16_t send_wait_timeout;
    void *global_user_ctx;
    httpd_free_ctx_fn_t global_user_ctx_free_fn;
    void *global_transport_ctx;
    httpd_free_ctx_fn_t global_transport_ctx_free_fn;
    bool enable_so_linger;
    int linger_timeout;
    bool keep_alive_enable;
    int keep_alive_idle;
    int keep_alive_interval;
    int keep_alive_count;
    httpd_open_func_t open_fn;
    httpd_close_func_t close_fn;
    void *uri_match_fn;
} httpd_config_t;

#define HTTPD_DEFAULT_CONFIG() \
    (httpd_config_t) { .task_priority = 5, .stack_size = 4096, .core_id = -1, \
                       .server_port = 80, .ctrl_port = 32768, .max_open_sockets = 7, \
                       .max_uri_handlers = 8, .max_resp_headers = 8, .backlog_conn = 5, \
                       .recv_wait_timeout = 5, .send_wait_timeout = 5 }

esp_err_t httpd_start(httpd_handle_t *handle, const httpd_config_t *config);
esp_err_t httpd_stop(httpd_handle_t handle);
esp_err_t httpd_register_uri_handler(httpd_handle_t handle, const httpd_uri_t *uri_handler);
esp_err_t httpd_queue_work(httpd_handle_t handle, httpd_work_fn_t work, void *arg);
void *httpd_sess_get_ctx(httpd_handle_t handle, int sockfd);
void httpd_sess_set_ctx(httpd_handle_t handle, int sockfd, void *ctx, httpd_free_ctx_fn_t free_fn);
esp_err_t httpd_sess_trigger_close(httpd_handle_t handle, int sockfd);
int httpd_req_to_sockfd(httpd_req_t *req);
void *httpd_get_global_user_ctx(httpd_handle_t handle);
esp_err_t httpd_ws_recv_frame(httpd_req_t *req, httpd_ws_frame_t *frame, size_t max_len);
esp_err_t httpd_ws_send_frame_async(httpd_handle_t handle, int sockfd, httpd_ws_frame_t *frame);

#ifdef __cplusplus
}
#endif
''',
    "esp_log.h": r'''// Stub of ESP-IDF esp_log.h, materialized by check-includes from esp_stubs.py.
#pragma once

// The real header provides the PRI* format macros transitively; ESP-IDF code
// conventionally uses them with only esp_log.h included.
#include <inttypes.h>
#include <stdint.h>

typedef enum {
    ESP_LOG_NONE,
    ESP_LOG_ERROR,
    ESP_LOG_WARN,
    ESP_LOG_INFO,
    ESP_LOG_DEBUG,
    ESP_LOG_VERBOSE,
} esp_log_level_t;

#ifdef __cplusplus
extern "C" {
#endif

void esp_log_write(esp_log_level_t level, const char *tag, const char *format, ...);
void esp_log_level_set(const char *tag, esp_log_level_t level);
uint32_t esp_log_timestamp(void);

#ifdef __cplusplus
}
#endif

#define ESP_LOGE(tag, format, ...) esp_log_write(ESP_LOG_ERROR, tag, format, ##__VA_ARGS__)
#define ESP_LOGW(tag, format, ...) esp_log_write(ESP_LOG_WARN, tag, format, ##__VA_ARGS__)
#define ESP_LOGI(tag, format, ...) esp_log_write(ESP_LOG_INFO, tag, format, ##__VA_ARGS__)
#define ESP_LOGD(tag, format, ...) esp_log_write(ESP_LOG_DEBUG, tag, format, ##__VA_ARGS__)
#define ESP_LOGV(tag, format, ...) esp_log_write(ESP_LOG_VERBOSE, tag, format, ##__VA_ARGS__)
''',
    "esp_mac.h": r'''// Stub of ESP-IDF esp_mac.h, materialized by check-includes from esp_stubs.py.
#pragma once

#include <stdint.h>

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    ESP_MAC_WIFI_STA,
    ESP_MAC_WIFI_SOFTAP,
    ESP_MAC_BT,
    ESP_MAC_ETH,
    ESP_MAC_BASE,
} esp_mac_type_t;

esp_err_t esp_read_mac(uint8_t *mac, esp_mac_type_t type);

#ifdef __cplusplus
}
#endif
''',
    "esp_memory_utils.h": r'''// Stub of ESP-IDF esp_memory_utils.h, materialized by check-includes from esp_stubs.py.
#pragma once

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

bool esp_ptr_external_ram(const void *p);
bool esp_ptr_internal(const void *p);

#ifdef __cplusplus
}
#endif
''',
    "esp_netif.h": r'''// Stub of ESP-IDF esp_netif.h, materialized by check-includes from esp_stubs.py.
#pragma once

#include <stdint.h>

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct esp_netif_obj esp_netif_t;

esp_netif_t *esp_netif_get_default_netif(void);
esp_err_t esp_netif_get_mac(esp_netif_t *esp_netif, uint8_t mac[6]);

#ifdef __cplusplus
}
#endif
''',
    "esp_pthread.h": r'''// Stub of ESP-IDF esp_pthread.h, materialized by check-includes from esp_stubs.py.
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    size_t stack_size;
    size_t prio;
    bool inherit_cfg;
    const char *thread_name;
    int pin_to_core;
    uint32_t stack_alloc_caps;
} esp_pthread_cfg_t;

esp_pthread_cfg_t esp_pthread_get_default_config(void);
esp_err_t esp_pthread_set_cfg(const esp_pthread_cfg_t *cfg);
esp_err_t esp_pthread_get_cfg(esp_pthread_cfg_t *p);

#ifdef __cplusplus
}
#endif
''',
    "esp_random.h": r'''// Stub of ESP-IDF esp_random.h, materialized by check-includes from esp_stubs.py.
#pragma once

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

uint32_t esp_random(void);
void esp_fill_random(void *buf, size_t len);

#ifdef __cplusplus
}
#endif
''',
    "esp_system.h": r'''// Stub of ESP-IDF esp_system.h, materialized by check-includes from esp_stubs.py.
#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

uint32_t esp_get_free_heap_size(void);
uint32_t esp_get_minimum_free_heap_size(void);

#ifdef __cplusplus
}
#endif
''',
    "esp_timer.h": r'''// Stub of ESP-IDF esp_timer.h, materialized by check-includes from esp_stubs.py.
#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

int64_t esp_timer_get_time(void);

#ifdef __cplusplus
}
#endif
''',
    "esp_websocket_client.h": r'''// Stub of ESP-IDF esp_websocket_client.h, materialized by check-includes from esp_stubs.py.
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "esp_err.h"
// The real header pulls in FreeRTOS via esp_transport; mirror that so code
// compiled against real ESP-IDF also parses here. Not exported: direct users
// of FreeRTOS symbols are still told to include the freertos headers.
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef void *esp_websocket_client_handle_t;
typedef const char *esp_event_base_t;
typedef void (*esp_event_handler_t)(void *handler_arg, esp_event_base_t base, int32_t event_id,
                                    void *event_data);

typedef enum {
    WEBSOCKET_EVENT_ANY = -1,
    WEBSOCKET_EVENT_ERROR = 0,
    WEBSOCKET_EVENT_CONNECTED,
    WEBSOCKET_EVENT_DISCONNECTED,
    WEBSOCKET_EVENT_DATA,
    WEBSOCKET_EVENT_CLOSED,
    WEBSOCKET_EVENT_BEFORE_CONNECT,
    WEBSOCKET_EVENT_BEGIN,
    WEBSOCKET_EVENT_FINISH,
} esp_websocket_event_id_t;

typedef struct {
    const char *data_ptr;
    int data_len;
    bool fin;
    uint8_t op_code;
    esp_websocket_client_handle_t client;
    void *user_context;
    int payload_len;
    int payload_offset;
} esp_websocket_event_data_t;

typedef struct {
    const char *uri;
    const char *host;
    int port;
    const char *path;
    bool disable_auto_reconnect;
    void *user_context;
    int task_prio;
    const char *task_name;
    int task_stack;
    int buffer_size;
    int network_timeout_ms;
    int reconnect_timeout_ms;
    int ping_interval_sec;
    int pingpong_timeout_sec;
    bool disable_pingpong_discon;
    const char *subprotocol;
    const char *headers;
    uint32_t task_caps;
} esp_websocket_client_config_t;

esp_websocket_client_handle_t esp_websocket_client_init(
    const esp_websocket_client_config_t *config);
esp_err_t esp_websocket_client_start(esp_websocket_client_handle_t client);
esp_err_t esp_websocket_client_stop(esp_websocket_client_handle_t client);
esp_err_t esp_websocket_client_destroy(esp_websocket_client_handle_t client);
int esp_websocket_client_send_text(esp_websocket_client_handle_t client, const char *data, int len,
                                   uint32_t timeout);
int esp_websocket_client_send_bin(esp_websocket_client_handle_t client, const char *data, int len,
                                  uint32_t timeout);
bool esp_websocket_client_is_connected(esp_websocket_client_handle_t client);
esp_err_t esp_websocket_register_events(esp_websocket_client_handle_t client,
                                        esp_websocket_event_id_t event,
                                        esp_event_handler_t event_handler, void *event_handler_arg);

#ifdef __cplusplus
}
#endif
''',
    "freertos/FreeRTOS.h": r'''// Stub of ESP-IDF freertos/FreeRTOS.h, materialized by check-includes from esp_stubs.py.
#pragma once

#include <stdint.h>

typedef uint32_t TickType_t;
typedef int BaseType_t;
typedef unsigned int UBaseType_t;

#define pdFALSE ((BaseType_t)0)
#define pdTRUE ((BaseType_t)1)
#define pdPASS pdTRUE
#define pdFAIL pdFALSE

#define configTICK_RATE_HZ 100
#define portMAX_DELAY ((TickType_t)0xFFFFFFFFU)
#define portTICK_PERIOD_MS ((TickType_t)(1000 / configTICK_RATE_HZ))
#define pdMS_TO_TICKS(ms) ((TickType_t)(((uint64_t)(ms) * configTICK_RATE_HZ) / 1000))
#define pdTICKS_TO_MS(ticks) ((TickType_t)(((uint64_t)(ticks) * 1000) / configTICK_RATE_HZ))
''',
    "freertos/event_groups.h": r'''// Stub of ESP-IDF freertos/event_groups.h, materialized by check-includes from esp_stubs.py.
#pragma once

#include "freertos/FreeRTOS.h"

typedef void *EventGroupHandle_t;
typedef TickType_t EventBits_t;

#ifdef __cplusplus
extern "C" {
#endif

EventGroupHandle_t xEventGroupCreate(void);
void vEventGroupDelete(EventGroupHandle_t event_group);
EventBits_t xEventGroupSetBits(EventGroupHandle_t event_group, EventBits_t bits);
EventBits_t xEventGroupClearBits(EventGroupHandle_t event_group, EventBits_t bits);
EventBits_t xEventGroupGetBits(EventGroupHandle_t event_group);
EventBits_t xEventGroupWaitBits(EventGroupHandle_t event_group, EventBits_t bits,
                                BaseType_t clear_on_exit, BaseType_t wait_for_all,
                                TickType_t ticks_to_wait);

#ifdef __cplusplus
}
#endif
''',
    "freertos/queue.h": r'''// Stub of ESP-IDF freertos/queue.h, materialized by check-includes from esp_stubs.py.
#pragma once

#include <stdint.h>

#include "freertos/FreeRTOS.h"

typedef void *QueueHandle_t;

#ifdef __cplusplus
extern "C" {
#endif

QueueHandle_t xQueueCreate(UBaseType_t length, UBaseType_t item_size);
QueueHandle_t xQueueCreateWithCaps(UBaseType_t length, UBaseType_t item_size, uint32_t caps);
void vQueueDelete(QueueHandle_t queue);
void vQueueDeleteWithCaps(QueueHandle_t queue);
BaseType_t xQueueSend(QueueHandle_t queue, const void *item, TickType_t ticks_to_wait);
BaseType_t xQueueReceive(QueueHandle_t queue, void *buffer, TickType_t ticks_to_wait);
BaseType_t xQueueReset(QueueHandle_t queue);
UBaseType_t uxQueueMessagesWaiting(QueueHandle_t queue);

#ifdef __cplusplus
}
#endif
''',
    "freertos/ringbuf.h": r'''// Stub of ESP-IDF freertos/ringbuf.h, materialized by check-includes from esp_stubs.py.
#pragma once

#include <stddef.h>
#include <stdint.h>

#include "freertos/FreeRTOS.h"

typedef void *RingbufHandle_t;
typedef struct xSTATIC_RINGBUFFER {
    void *dummy[24];
} StaticRingbuffer_t;

typedef enum {
    RINGBUF_TYPE_NOSPLIT,
    RINGBUF_TYPE_ALLOWSPLIT,
    RINGBUF_TYPE_BYTEBUF,
} RingbufferType_t;

#ifdef __cplusplus
extern "C" {
#endif

RingbufHandle_t xRingbufferCreate(size_t size, RingbufferType_t type);
RingbufHandle_t xRingbufferCreateWithCaps(size_t size, RingbufferType_t type, uint32_t caps);
void vRingbufferDelete(RingbufHandle_t ringbuf);
RingbufHandle_t xRingbufferCreateStatic(size_t size, RingbufferType_t type, uint8_t *storage,
                                        StaticRingbuffer_t *data);
BaseType_t xRingbufferSend(RingbufHandle_t ringbuf, const void *item, size_t size,
                           TickType_t ticks_to_wait);
BaseType_t xRingbufferSendAcquire(RingbufHandle_t ringbuf, void **item, size_t size,
                                  TickType_t ticks_to_wait);
BaseType_t xRingbufferSendComplete(RingbufHandle_t ringbuf, void *item);
void vRingbufferGetInfo(RingbufHandle_t ringbuf, UBaseType_t *free_index,
                        UBaseType_t *read_index, UBaseType_t *write_index,
                        UBaseType_t *acquire_index, UBaseType_t *items_waiting);
void *xRingbufferReceive(RingbufHandle_t ringbuf, size_t *item_size, TickType_t ticks_to_wait);
void *xRingbufferReceiveUpTo(RingbufHandle_t ringbuf, size_t *item_size, TickType_t ticks_to_wait,
                             size_t max_size);
void vRingbufferReturnItem(RingbufHandle_t ringbuf, void *item);
size_t xRingbufferGetCurFreeSize(RingbufHandle_t ringbuf);

#ifdef __cplusplus
}
#endif
''',
    "freertos/semphr.h": r'''// Stub of ESP-IDF freertos/semphr.h, materialized by check-includes from esp_stubs.py.
#pragma once

#include "freertos/FreeRTOS.h"

typedef void *SemaphoreHandle_t;

#ifdef __cplusplus
extern "C" {
#endif

SemaphoreHandle_t xSemaphoreCreateBinary(void);
SemaphoreHandle_t xSemaphoreCreateMutex(void);
SemaphoreHandle_t xSemaphoreCreateCounting(UBaseType_t max_count, UBaseType_t initial_count);
BaseType_t xSemaphoreTake(SemaphoreHandle_t semaphore, TickType_t ticks_to_wait);
BaseType_t xSemaphoreGive(SemaphoreHandle_t semaphore);
void vSemaphoreDelete(SemaphoreHandle_t semaphore);

#ifdef __cplusplus
}
#endif
''',
    "freertos/task.h": r'''// Stub of ESP-IDF freertos/task.h, materialized by check-includes from esp_stubs.py.
#pragma once

#include <stdint.h>

#include "freertos/FreeRTOS.h"

typedef void *TaskHandle_t;
typedef void (*TaskFunction_t)(void *);

#ifdef __cplusplus
extern "C" {
#endif

BaseType_t xTaskCreate(TaskFunction_t task, const char *name, uint32_t stack_depth,
                       void *parameters, UBaseType_t priority, TaskHandle_t *created_task);
BaseType_t xTaskCreatePinnedToCore(TaskFunction_t task, const char *name, uint32_t stack_depth,
                                   void *parameters, UBaseType_t priority,
                                   TaskHandle_t *created_task, BaseType_t core_id);
void vTaskDelete(TaskHandle_t task);
void vTaskDelay(TickType_t ticks);
void vTaskSuspend(TaskHandle_t task);
void vTaskResume(TaskHandle_t task);
TickType_t xTaskGetTickCount(void);
UBaseType_t uxTaskGetStackHighWaterMark(TaskHandle_t task);
BaseType_t xPortGetCoreID(void);
void vTaskYield(void);

#ifdef __cplusplus
}
#endif

// Real task.h defines this as a macro wrapping portYIELD().
#define taskYIELD() vTaskYield()
''',
    "lwip/netdb.h": r'''// Stub of ESP-IDF lwip/netdb.h, materialized by check-includes from esp_stubs.py.
//
// Same re-export approach as lwip/sockets.h: on ESP this header provides
// getaddrinfo()/gethostbyname(); the host's netdb.h supplies the symbols.
#pragma once

#include <netdb.h>  // IWYU pragma: export
''',
    "lwip/sockets.h": r'''// Stub of ESP-IDF lwip/sockets.h, materialized by check-includes from esp_stubs.py.
//
// On ESP, lwip/sockets.h is the canonical provider of the BSD socket API.
// Re-exporting the host's POSIX headers gives the same symbols with correct
// attribution: code that directly includes <lwip/sockets.h> is credited with
// providing close()/setsockopt()/etc.
#pragma once

#include <netinet/in.h>   // IWYU pragma: export
#include <netinet/tcp.h>  // IWYU pragma: export
#include <sys/socket.h>   // IWYU pragma: export
#include <unistd.h>       // IWYU pragma: export
''',
    "mbedtls/base64.h": r'''// Stub of mbedtls/base64.h (ESP-IDF bundled mbedtls), materialized by check-includes from esp_stubs.py.
#pragma once

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

int mbedtls_base64_encode(unsigned char *dst, size_t dlen, size_t *olen,
                          const unsigned char *src, size_t slen);
int mbedtls_base64_decode(unsigned char *dst, size_t dlen, size_t *olen,
                          const unsigned char *src, size_t slen);

#ifdef __cplusplus
}
#endif
''',
    "sdkconfig.h": r'''// Stub of the ESP-IDF build-generated sdkconfig.h, materialized by check-includes from esp_stubs.py.
//
// Intentionally empty: CONFIG_* options evaluate as unset, matching a
// default configuration. The checker never reports this header as unused
// (its macros are invisible to usage analysis when unset), so including it
// for CONFIG_* access is always safe.
#pragma once
''',
}
