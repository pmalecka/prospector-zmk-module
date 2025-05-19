// #include <zephyr/kernel.h>
// #include <zephyr/device.h>
// #include <zephyr/drivers/display.h>

// #include <zephyr/usb/usb_device.h>
// #include <zephyr/drivers/pwm.h>
// #include <zephyr/drivers/led.h>

// #include <zmk/display.h>

// #include <zephyr/logging/log.h>
// LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

// static const struct device *pwm_leds_dev = DEVICE_DT_GET_ONE(pwm_leds);
// #define DISP_BL DT_NODE_CHILD_IDX(DT_NODELABEL(disp_bl))

// struct display_sleep_state {
//     bool on;
// };

// static struct display_sleep_state state = {.on = true};

// static int zmk_display_sleep_update(void) {
//     uint8_t brt = zmk_backlight_get_brt();
//     LOG_DBG("Update display sleep: %d%%", brt);

//     for (int i = 0; i < BACKLIGHT_NUM_LEDS; i++) {
//         int rc = led_set_brightness(backlight_dev, i, brt);
//         if (rc != 0) {
//             LOG_ERR("Failed to update backlight LED %d: %d", i, rc);
//             return rc;
//         }
//     }
//     return 0;
// }


// static int display_sleep_auto_state(bool *prev_state, bool new_state) {
//     if (state.on == new_state) {
//         return 0;
//     }
//     state.on = new_state && *prev_state;
//     *prev_state = !new_state;
//     return zmk_display_sleep_update();
// }

// static int display_sleep_event_listener(const zmk_event_t *eh) {
//     if (as_zmk_activity_state_changed(eh)) {
//         static bool prev_state = false;
//         return display_sleep_auto_state(&prev_state, zmk_activity_get_state() == ZMK_ACTIVITY_ACTIVE);
//     }
// }

// ZMK_LISTENER(display_sleep, display_sleep_event_listener);

// ZMK_SUBSCRIPTION(display_sleep, zmk_activity_state_changed);

// SYS_INIT(display_sleep_init, APPLICATION, CONFIG_APPLICATION_INIT_PRIORITY);

// // #define DISP_BL DT_NODE_CHILD_IDX(DT_NODELABEL(disp_bl))

// // static bool host_sleep_pending;

// // static void handle_host_sleep(bool sleep) {
// //     const struct device *display = DEVICE_DT_GET(DT_CHOSEN(zephyr_display));
// //     static const struct device *pwm_leds_dev = DEVICE_DT_GET_ONE(pwm_leds);

// //     if (!device_is_ready(display)) {
// //         LOG_INF("DISPLAY DEVICE NOT READY");
// //         return;
// //     }

// //     if(sleep) {
// //         display_blanking_on(display);
// //         led_set_brightness(pwm_leds_dev, DISP_BL, 0);
// //         // lv_disp_set_rotation(lv_disp_get_default(), LV_DISP_ROT_NONE);
// //     } else {
// //         display_blanking_off(display);
// //         // lv_task_resume(lv_task_get_default());
// //     }
// // }

// // static void sleep_debounce_fn(struct k_work *work) {
// //     host_sleep_pending = false;
// //     handle_host_sleep(true);
// // }

// // // Statically define a delayable work item with the handler
// // K_WORK_DELAYABLE_DEFINE(sleep_debounce_work, sleep_debounce_fn);

// // void usb_host_status_cb(enum usb_dc_status_code status, const uint8_t *param) {
// //     // Start-of-frame events are too frequent
// //     if (status == USB_DC_SOF) {
// //         return;
// //     }
    
// //     LOG_INF("USB HOST STATUS CHANGED TO '%d'", status);

// //     switch(status) {
// //     case USB_DC_SUSPEND:
// //         LOG_INF("USB HOST STATUS CHANGED TO: SUSPEND");
// //         if(!host_sleep_pending) {
// //             k_work_reschedule(&sleep_debounce_work, 
// //                             K_MSEC(CONFIG_PROSPECTOR_SLEEP_DEBOUNCE_MS));
// //             host_sleep_pending = true;
// //         }
// //         break;
// //     case USB_DC_RESUME:
// //         LOG_INF("USB HOST STATUS CHANGED TO: RESUME");
// //         k_work_cancel_delayable(&sleep_debounce_work);
// //         host_sleep_pending = false;
// //         handle_host_sleep(false);
// //         break;
// //     default: break;
// //     }
// // }

// // int init_display_sleep(void)
// // {
// //     LOG_INF("!!JAJA INIT DISPLAY SLEEP!!");
// //     #ifdef CONFIG_PROSPECTOR_DISPLAY_SLEEP
// //     LOG_INF("ATTACHING_USB_HOST_CALLBACK");
// //     usb_dc_set_status_callback(usb_host_status_cb);
// //     #endif

// // 	return 0;
// // }

// // SYS_INIT(init_display_sleep, APPLICATION, 60);