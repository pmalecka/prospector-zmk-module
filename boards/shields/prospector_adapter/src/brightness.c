#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/drivers/pwm.h>
#include <zephyr/drivers/led.h>
#include <zephyr/sys/printk.h>

#include <zmk/usb.h>
#include <zmk/display.h>
#include <zephyr/drivers/display.h>

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(als, 4);

static const struct device *pwm_leds_dev = DEVICE_DT_GET_ONE(pwm_leds);
#define DISP_BL DT_NODE_CHILD_IDX(DT_NODELABEL(disp_bl))

#ifdef CONFIG_PROSPECTOR_USE_AMBIENT_LIGHT_SENSOR
const struct device *display = DEVICE_DT_GET(DT_CHOSEN(zephyr_display));

static bool suspended;

static uint8_t current_brightness = 100;

#define SENSOR_MIN      0       // Minimum sensor reading
#define SENSOR_MAX      100   // Maximum sensor reading
#define PWM_MIN         1       // Minimum PWM duty cycle (%) - keep display visible
#define PWM_MAX         100     // Maximum PWM duty cycle (%)

#define FADE_STEP                        1
#define FADE_SLEEP_BRIGHTEN_MS           3
#define FADE_SLEEP_DARKEN_MS             10
#define FADE_THRESHOLD                   10

#define NORMAL_SAMPLE_SLEEP_MS           100

#define BURST_SAMPLE_SLEEP_MS            30
#define BURST_SAMPLE_TIMEOUT             10
#define BURST_SAMPLE_CONSECUTIVE         3

void control_display(bool turn_off) {

    if(turn_off) {
        // turn off the display and the backlight
        display_blanking_on(display);
        led_off(pwm_leds_dev, DISP_BL);
    } else {
        // turn on the display and the backlight
        display_blanking_off(display);
        led_on(pwm_leds_dev, DISP_BL);
    }
}

// TODO: rewrite this, so that it supports also mode, where the ambient light sensor
// is not available. Consider using ZMK events, that will let this thread know, that
// usb connection status has changed, instead of cramming 2 things into 1 file ;-)
void handle_usb_suspend_state() {

    enum usb_dc_status_code dc_status = zmk_usb_get_status();

    uint8_t should_be_suspended = -EAGAIN;

    switch (dc_status) {
        case USB_DC_RESET:
            LOG_INF("Prospector USB handler: Device reset");
            should_be_suspended = 0;
            break;
        case USB_DC_DISCONNECTED:
            LOG_INF("Prospector USB handler: Device disconnected");
            should_be_suspended = 0;
            break;
        case USB_DC_SUSPEND:
            LOG_INF("Prospector USB handler: Device suspended");
            should_be_suspended = 1;
            break;
        case USB_DC_RESUME:
            if (suspended) {
                LOG_INF("Prospector USB handler: Device resumed from sleep");
                should_be_suspended = 0;
            } else {
                LOG_DBG("Prospector USB handler: Device resumed spuriously");
            }
            break;
        default:
            LOG_INF("Prospector USB handler: Other event '%d'", dc_status);
            break;
    }

    // TODO: refactor this spaghetti :/
    // figure out if the display should be turned off or on
    if (suspended && should_be_suspended == 0) {
        // if the display was suspended previously and now it should not be suspended (should be ON)
        // then turn it ON
        control_display(true);
        suspended = false;
    } else if (!suspended && should_be_suspended == 1) {
        // if the display was NOT suspended previously and now it should be suspended (should be OFF)
        // then turn it OFF
        control_display(false);
        suspended = true;
    }
}

// TODO: add debounce for these suspend events..
// K_WORK_DELAYABLE_DEFINE(sleep_debounce_work, sleep_debounce_fn);
// static bool host_sleep_pending;

// static void handle_host_sleep(bool sleep) {

//     if(sleep) {
//         display_blanking_on(display);
//     } else {
//         display_blanking_off(display);
//     }
// }

// static void sleep_debounce_fn(struct k_work *work) {
//     host_sleep_pending = false;
//     handle_host_sleep(nrfx_usbd_bus_suspend_check());
// }

// void usb_host_status_cb(enum usb_dc_status_code status, const uint8_t *param) {
//     switch(status) {
//     case USB_DC_SUSPEND:
//         if(!host_sleep_pending) {
//             k_work_reschedule(&sleep_debounce_work, 
//                             K_MSEC(CONFIG_PROSPECTOR_SLEEP_DEBOUNCE_MS));
//             host_sleep_pending = true;
//         }
//         break;
//     case USB_DC_RESUME:
//         k_work_cancel_delayable(&sleep_debounce_work);
//         host_sleep_pending = false;
//         handle_host_sleep(false);
//         break;
//     default: break;
//     }
// }

uint8_t map_light_to_pwm(int32_t sensor_reading) {
    
    // Handle invalid/error readings
    if (sensor_reading < SENSOR_MIN) {
        return PWM_MIN;  // Default to minimum brightness on error
    }

    // Clamp to maximum
    if (sensor_reading > SENSOR_MAX) {
        sensor_reading = SENSOR_MAX;
    }

    // Linear mapping
    uint8_t pwm_value = (uint8_t)(
        PWM_MIN + ((PWM_MAX - PWM_MIN) *
        (sensor_reading - SENSOR_MIN)) / (SENSOR_MAX - SENSOR_MIN)
    );

    return pwm_value;
}

uint8_t bl_fade(uint8_t source, uint8_t target) {
    bool increasing = target > source;

    while ((increasing && current_brightness < target) ||
           (!increasing && current_brightness > target)) {

        if (led_set_brightness(pwm_leds_dev, DISP_BL, current_brightness)) {
            LOG_ERR("Failed to set brightness");
        }

        current_brightness += increasing ? FADE_STEP : -FADE_STEP;

        // Ensure we don't overshoot bounds
        if (current_brightness > 100) {
            current_brightness = 100;
        } else if (current_brightness < 0) {
            current_brightness = 0;
        }

        k_msleep(increasing ? FADE_SLEEP_BRIGHTEN_MS : FADE_SLEEP_DARKEN_MS);
    }

    return 0;
}

extern void als_thread(void *d0, void *d1, void *d2) {
    ARG_UNUSED(d0);
    ARG_UNUSED(d1);
    ARG_UNUSED(d2);

    LOG_INF("ALS THREAD STARTED!!!");

    const struct device *dev;
    struct sensor_value intensity;
    uint8_t mapped_brightness;

    dev = DEVICE_DT_GET_ONE(avago_apds9960);
    if (!device_is_ready(dev)) {
        printk("sensor: device not ready.\n");
    }

    // led_set_brightness(pwm_leds_dev, DISP_BL, 100);

    while (1) {

        k_msleep(NORMAL_SAMPLE_SLEEP_MS);

        // the handle_usb_suspend_state function is only interested in the transitions, when the
        // host wakes up from sleep and when the host goes to sleep. It shouldn't run
        // continuously, as this practically circumvents the als autobrightness..
        handle_usb_suspend_state();
        // the als autobrightness thread needs to know, whether the host is asleep or not
        // and it needs to know this on a continuous basis.. 
        if (suspended) {
            LOG_INF("USB is suspended, skipping brightness adjustment");
            continue;
        }

        if (sensor_sample_fetch(dev)) {
            LOG_ERR("sensor_sample fetch failed\n");
        }

        if (sensor_channel_get(dev, SENSOR_CHAN_LIGHT, &intensity)) {
            LOG_ERR("Cannot read ALS data.\n");
        }

        // LOG_INF("ambient light intensity %d", intensity.val1);

        mapped_brightness = map_light_to_pwm(intensity.val1);
        // LOG_INF("NORMAL: mapped PWM duty cycle %d\n", mapped_brightness);

        if (abs(mapped_brightness - current_brightness) > FADE_THRESHOLD) {
            uint8_t integrator = 0;

            for (int i = 0; i < BURST_SAMPLE_TIMEOUT; i++) {
                k_msleep(BURST_SAMPLE_SLEEP_MS);

                if (sensor_sample_fetch(dev)) {
                    LOG_ERR("sensor_sample fetch failed\n");
                }
                if (sensor_channel_get(dev, SENSOR_CHAN_LIGHT, &intensity)) {
                    LOG_ERR("Cannot read ALS data.\n");
                }

                mapped_brightness = map_light_to_pwm(intensity.val1);
                // LOG_INF("BURST: mapped PWM duty cycle %d\n", mapped_brightness);

                if (abs(mapped_brightness - current_brightness) > FADE_THRESHOLD) {
                    integrator++;
                    // printk("integrator at: %d", integrator);
                    if (integrator >= BURST_SAMPLE_CONSECUTIVE) {
                        bl_fade(current_brightness, mapped_brightness);
                        current_brightness = mapped_brightness;
                        // LOG_INF("SETTING NEW BRIGHTNESS: %d", mapped_brightness);
                        break;
                    }
                }
            }
        }
        // led_set_brightness(pwm_leds_dev, DISP_BL, map_light_to_pwm(intensity.val1));
    }
}

K_THREAD_DEFINE(als_tid, 1024, als_thread, NULL, NULL, NULL, K_LOWEST_APPLICATION_THREAD_PRIO, 0,
                0);

#else

static int init_fixed_brightness(void) {
    led_set_brightness(pwm_leds_dev, DISP_BL, CONFIG_PROSPECTOR_FIXED_BRIGHTNESS);

    return 0;
}

SYS_INIT(init_fixed_brightness, APPLICATION, CONFIG_APPLICATION_INIT_PRIORITY);

#endif