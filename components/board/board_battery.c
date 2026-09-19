#include "board.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"

static adc_oneshot_unit_handle_t s_adc;
static adc_cali_handle_t s_cali;

int board_battery_mv(void)
{
    if (!s_adc) {
        adc_oneshot_unit_init_cfg_t u = {.unit_id = ADC_UNIT_1};
        if (adc_oneshot_new_unit(&u, &s_adc) != ESP_OK) return -1;
        adc_oneshot_chan_cfg_t c = {.atten = ADC_ATTEN_DB_12, .bitwidth = ADC_BITWIDTH_DEFAULT};
        adc_oneshot_config_channel(s_adc, ADC_CHANNEL_8, &c);
        adc_cali_curve_fitting_config_t cc = {
            .unit_id = ADC_UNIT_1, .chan = ADC_CHANNEL_8,
            .atten = ADC_ATTEN_DB_12, .bitwidth = ADC_BITWIDTH_DEFAULT};
        adc_cali_create_scheme_curve_fitting(&cc, &s_cali);
    }
    int raw = 0, mv = 0, sum = 0;
    for (int i = 0; i < 8; i++) {
        adc_oneshot_read(s_adc, ADC_CHANNEL_8, &raw);
        if (s_cali) adc_cali_raw_to_voltage(s_cali, raw, &mv);
        sum += mv;
    }
    return sum / 8 * 2;  // x2 divider
}
