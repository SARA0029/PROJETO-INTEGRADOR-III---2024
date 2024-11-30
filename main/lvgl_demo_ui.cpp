
#include "lvgl.h"
#define LV_FONT_MONTSERRAT_16 0

void example_lvgl_demo_ui()
{

    lv_obj_t *scr = lv_disp_get_scr_act(nullptr);
    lv_obj_t *label = lv_label_create(scr);
    lv_label_set_long_mode(label, LV_LABEL_LONG_SCROLL_CIRCULAR); /* Circular scroll */
    lv_label_set_text(label, "Hello Espressif, Hello LVGL.");
    /* Size of the screen (if you use rotation 90 or 270, please set disp->driver->ver_res) */
    lv_obj_set_width(label, 128);
    lv_obj_align(label, LV_ALIGN_TOP_MID, 0, 0);
}
