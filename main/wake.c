#include "wake.h"
#include "board.h"
#include "story_mem.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_wn_iface.h"
#include "esp_wn_models.h"
#include "model_path.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

static const char *TAG = "wake";

static srmodel_list_t *s_models;
static const esp_wn_iface_t *s_wn;
static char *s_name;
static model_iface_data_t *s_data;
static TaskHandle_t s_task;
static SemaphoreHandle_t s_done;
static volatile bool s_run;
static wake_cb_t s_cb;

bool wake_init(void)
{
    s_models = esp_srmodel_init("model");
    if (!s_models) {
        ESP_LOGE(TAG, "no esp-sr models in the 'model' partition");
        return false;
    }
    s_name = esp_srmodel_filter(s_models, ESP_WN_PREFIX, NULL);
    if (!s_name) {
        ESP_LOGE(TAG, "no WakeNet model found");
        return false;
    }
    s_wn = esp_wn_handle_from_name(s_name);
    s_done = xSemaphoreCreateBinary();
    ESP_LOGI(TAG, "WakeNet model %s (\"%s\")", s_name, WAKE_PHRASE);
    return s_wn != NULL;
}

const char *wake_model_name(void) { return s_name ? s_name : "none"; }
bool wake_running(void) { return s_task != NULL; }

static volatile bool s_monitor;
void wake_set_monitor(bool on) { s_monitor = on; }

static void wake_task(void *arg)
{
    (void)arg;
    int chunk = s_wn->get_samp_chunksize(s_data);
    int16_t *buf = heap_caps_malloc((size_t)chunk * sizeof(int16_t), MALLOC_CAP_INTERNAL);
    board_mic_start();
    int64_t mon_t0 = 0;
    int mon_peak = 0, mon_chunks = 0;
    while (s_run && buf) {
        size_t got = 0;
        while (got < (size_t)chunk && s_run) got += board_mic_read(buf + got, (size_t)chunk - got, 100);
        if (!s_run) break;
        if (s_monitor) {
            // "wakemon on": is the detector actually hearing anything? Peak
            // level per second, so a silent or dead mic is obvious.
            for (int i = 0; i < chunk; i++) {
                int a = buf[i] < 0 ? -buf[i] : buf[i];
                if (a > mon_peak) mon_peak = a;
            }
            mon_chunks++;
            int64_t now = story_time_us();
            if (!mon_t0) mon_t0 = now;
            else if (now - mon_t0 >= 1000000) {
                ESP_LOGI(TAG, "mic peak %5d (%2d%% of full scale) over %d chunks", mon_peak,
                         mon_peak * 100 / 32768, mon_chunks);
                mon_t0 = now;
                mon_peak = mon_chunks = 0;
            }
        }
        if (s_wn->detect(s_data, buf) == WAKENET_DETECTED) {
            ESP_LOGI(TAG, "wake word detected: \"%s\"", WAKE_PHRASE);
            s_run = false;
            if (s_cb) s_cb();
        }
    }
    board_mic_stop();
    free(buf);
    xSemaphoreGive(s_done);
    vTaskDelete(NULL);
}

bool wake_start(wake_cb_t cb)
{
    if (!s_wn || s_task) return s_task != NULL;
    story_mem_snapshot_t before, after;
    story_mem_snapshot(&before);
    int64_t t0 = story_time_us();
    s_data = s_wn->create(s_name, DET_MODE_95);
    if (!s_data) {
        ESP_LOGE(TAG, "WakeNet create failed");
        story_mem_log("wake-fail");
        return false;
    }
    story_mem_snapshot(&after);
    ESP_LOGI(TAG, "listening for \"%s\" (create %lld ms, chunk %d, internal %d B, psram %d B)",
             WAKE_PHRASE, (story_time_us() - t0) / 1000, s_wn->get_samp_chunksize(s_data),
             (int)before.int_free - (int)after.int_free, (int)before.psram_free - (int)after.psram_free);
    s_cb = cb;
    s_run = true;
    if (xTaskCreatePinnedToCore(wake_task, "wake", 4096, NULL, 5, &s_task, 0) != pdPASS) {
        ESP_LOGE(TAG, "wake task create failed");
        s_wn->destroy(s_data);
        s_data = NULL;
        s_task = NULL;
        return false;
    }
    return true;
}

void wake_stop(void)
{
    if (!s_task) return;
    s_run = false;
    xSemaphoreTake(s_done, portMAX_DELAY);
    s_task = NULL;
    s_wn->destroy(s_data);
    s_data = NULL;
}
