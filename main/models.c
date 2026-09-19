#include "models.h"
#include "board.h"
#include "think.h"
#include "esp_log.h"
#include "nvs.h"
#include "nvs_flash.h"
#include <dirent.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>

static const char *TAG = "models";
#define ROOT BOARD_SD_MOUNT "/story"
#define GGUF_ROOT BOARD_SD_MOUNT "/models"

static model_info_t s_models[MODELS_MAX];
static int s_count, s_active = -1;

// Friendly names for the folders we ship, if they have no name.txt.
static const char *default_name(const char *dir)
{
    if (!strcmp(dir, "llm8m_kid")) return "TinyTalk 2 8M (kid+Gume)";
    if (!strcmp(dir, "llm8m")) return "TinyTalk 2 8M";
    if (!strcmp(dir, "llm")) return "TinyTalk 3M";
    return dir;
}

static bool file_size(const char *path, size_t *n)
{
    struct stat st;
    if (stat(path, &st) != 0) return false;
    *n = (size_t)st.st_size;
    return true;
}

static void load_saved(char *out, size_t cap)
{
    out[0] = 0;
    nvs_handle_t h;
    if (nvs_open("braino", NVS_READONLY, &h) != ESP_OK) return;
    size_t len = cap;
    if (nvs_get_str(h, "llm_dir", out, &len) != ESP_OK) out[0] = 0;
    nvs_close(h);
}

static void save(const char *dir)
{
    nvs_handle_t h;
    if (nvs_open("braino", NVS_READWRITE, &h) != ESP_OK) {
        ESP_LOGE(TAG, "NVS open failed; model choice not saved");
        return;
    }
    nvs_set_str(h, "llm_dir", dir);
    nvs_commit(h);
    nvs_close(h);
}

int models_scan(void)
{
    static bool nvs_ready;
    if (!nvs_ready) {
        esp_err_t e = nvs_flash_init();
        if (e == ESP_ERR_NVS_NO_FREE_PAGES || e == ESP_ERR_NVS_NEW_VERSION_FOUND) {
            nvs_flash_erase();
            e = nvs_flash_init();
        }
        nvs_ready = e == ESP_OK;
    }
    s_count = 0;
    DIR *d = opendir(ROOT);
    if (!d) {
        ESP_LOGE(TAG, "cannot open %s (SD card?)", ROOT);
        return 0;
    }
    struct dirent *e;
    while ((e = readdir(d)) && s_count < MODELS_MAX) {
        if (strncmp(e->d_name, "llm", 3) != 0) continue;
        model_info_t *m = &s_models[s_count];
        char p[128];
        size_t a, b;
        snprintf(m->dir, sizeof m->dir, ROOT "/%.40s", e->d_name);
        snprintf(p, sizeof p, "%s/model.bin", m->dir);
        if (!file_size(p, &a)) continue;
        snprintf(p, sizeof p, "%s/tok.bin", m->dir);
        if (!file_size(p, &b)) continue;
        m->bytes = a + b;
        snprintf(m->name, sizeof m->name, "%.39s", default_name(e->d_name));
        snprintf(p, sizeof p, "%s/name.txt", m->dir);
        FILE *f = fopen(p, "r");
        if (f) {
            if (fgets(m->name, sizeof m->name, f)) m->name[strcspn(m->name, "\r\n")] = 0;
            fclose(f);
        }
        s_count++;
    }
    closedir(d);

    // GGUF files (llama.cpp format) dropped into /sd/models/.
    DIR *g = opendir(GGUF_ROOT);
    while (g && (e = readdir(g)) && s_count < MODELS_MAX) {
        size_t nl = strlen(e->d_name);
        if (nl < 6 || strcasecmp(e->d_name + nl - 5, ".gguf") != 0) continue;
        model_info_t *m = &s_models[s_count];
        snprintf(m->dir, sizeof m->dir, GGUF_ROOT "/%.50s", e->d_name);
        if (!file_size(m->dir, &m->bytes)) continue;
        snprintf(m->name, sizeof m->name, "%.*s (GGUF)", (int)(nl - 5 > 30 ? 30 : nl - 5), e->d_name);
        s_count++;
    }
    if (g) closedir(g);

    // Stable order: by path (story folders first, then GGUF files).
    for (int i = 1; i < s_count; i++)
        for (int j = i; j > 0 && strcmp(s_models[j - 1].dir, s_models[j].dir) > 0; j--) {
            model_info_t t = s_models[j];
            s_models[j] = s_models[j - 1];
            s_models[j - 1] = t;
        }

    char saved[64];
    load_saved(saved, sizeof saved);
    s_active = -1;
    for (int i = 0; i < s_count; i++)
        if (!strcmp(s_models[i].dir, saved) || (!saved[0] && !strcmp(s_models[i].dir, think_model_dir())))
            s_active = i;
    if (s_active < 0 && s_count > 0) {
        // Keep the firmware default if present, else the first model.
        s_active = 0;
        for (int i = 0; i < s_count; i++)
            if (!strcmp(s_models[i].dir, think_model_dir())) s_active = i;
    }
    if (s_active >= 0) think_set_model_dir(s_models[s_active].dir);
    for (int i = 0; i < s_count; i++)
        ESP_LOGI(TAG, "%c %-24s %6.2f MB  %s", i == s_active ? '*' : ' ', s_models[i].name,
                 s_models[i].bytes / 1048576.0, s_models[i].dir);
    return s_count;
}

int models_count(void) { return s_count; }
const model_info_t *models_get(int i) { return (i >= 0 && i < s_count) ? &s_models[i] : NULL; }
int models_active(void) { return s_active; }

bool models_select(int i)
{
    if (i < 0 || i >= s_count) return false;
    s_active = i;
    think_set_model_dir(s_models[i].dir);
    save(s_models[i].dir);
    ESP_LOGI(TAG, "selected %s (%s)", s_models[i].name, s_models[i].dir);
    return true;
}
