#include "rg_system.h"
#include "rg_input.h"
#include "rg_usb_keyboard.h"

#include "usb/usb_host.h"
#include "usb/hid_host.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

#include <string.h>


// ============================================================================
// HID Usage IDs
// ============================================================================

#define HID_KEY_A          0x04
#define HID_KEY_B          0x05
#define HID_KEY_C          0x06
#define HID_KEY_D          0x07
#define HID_KEY_E          0x08
#define HID_KEY_F          0x09
#define HID_KEY_G          0x0A
#define HID_KEY_H          0x0B
#define HID_KEY_I          0x0C
#define HID_KEY_J          0x0D
#define HID_KEY_K          0x0E
#define HID_KEY_L          0x0F
#define HID_KEY_M          0x10
#define HID_KEY_N          0x11
#define HID_KEY_O          0x12
#define HID_KEY_P          0x13
#define HID_KEY_Q          0x14
#define HID_KEY_R          0x15
#define HID_KEY_S          0x16
#define HID_KEY_T          0x17
#define HID_KEY_U          0x18
#define HID_KEY_V          0x19
#define HID_KEY_W          0x1A
#define HID_KEY_X          0x1B
#define HID_KEY_Y          0x1C
#define HID_KEY_Z          0x1D

#define HID_KEY_ENTER      0x28
#define HID_KEY_ESCAPE     0x29
#define HID_KEY_BACKSPACE  0x2A
#define HID_KEY_TAB        0x2B
#define HID_KEY_SPACE      0x2C

#define HID_KEY_F1         0x3A
#define HID_KEY_F2         0x3B

#define HID_KEY_RIGHT      0x4F
#define HID_KEY_LEFT       0x50
#define HID_KEY_DOWN       0x51
#define HID_KEY_UP         0x52


// ============================================================================
// State
// ============================================================================

static volatile uint32_t usb_keyboard_state = 0;

static volatile bool usb_keyboard_ready = false;
static volatile bool usb_keyboard_connected = false;


// ============================================================================
// HID event queue
// ============================================================================
//
// IMPORTANT:
//
// HID callback НЕ виконує open/start/set_protocol/set_idle.
//
// Callback тільки кладе подію в queue.
// Вся робота з HID виконується в usb_hid_event_task().
//

typedef struct
{
    hid_host_device_handle_t handle;
    hid_host_driver_event_t event;

} usb_hid_event_t;


static QueueHandle_t usb_hid_event_queue = NULL;


// ============================================================================
// Task handles
// ============================================================================

static TaskHandle_t usb_host_task_handle = NULL;
static TaskHandle_t usb_hid_task_handle = NULL;
static TaskHandle_t usb_monitor_task_handle = NULL;


// ============================================================================
// HID -> Retro-Go key mapping
// ============================================================================

static uint32_t hid_to_rg_key(uint8_t code)
{
    switch (code)
    {
        // ------------------------------------------------------------
        // D-PAD
        // ------------------------------------------------------------

        case HID_KEY_UP:
        case HID_KEY_W:
            return RG_KEY_UP;

        case HID_KEY_DOWN:
        case HID_KEY_S:
            return RG_KEY_DOWN;

        case HID_KEY_LEFT:
        case HID_KEY_A:
            return RG_KEY_LEFT;

        case HID_KEY_RIGHT:
        case HID_KEY_D:
            return RG_KEY_RIGHT;


        // ------------------------------------------------------------
        // A
        // ------------------------------------------------------------

        case HID_KEY_Z:
        case HID_KEY_C:
            return RG_KEY_A;


        // ------------------------------------------------------------
        // B
        // ------------------------------------------------------------

        case HID_KEY_X:
        case HID_KEY_V:
            return RG_KEY_B;


        // ------------------------------------------------------------
        // START
        // ------------------------------------------------------------

        case HID_KEY_ENTER:
        case HID_KEY_SPACE:
            return RG_KEY_START;


        // ------------------------------------------------------------
        // SELECT
        // ------------------------------------------------------------

        case HID_KEY_ESCAPE:
        case HID_KEY_BACKSPACE:
            return RG_KEY_SELECT;


        // ------------------------------------------------------------
        // MENU
        // ------------------------------------------------------------

        case HID_KEY_M:
        case HID_KEY_F1:
            return RG_KEY_MENU;


        // ------------------------------------------------------------
        // OPTION
        // ------------------------------------------------------------

        case HID_KEY_O:
        case HID_KEY_F2:
            return RG_KEY_OPTION;


        default:
            return 0;
    }
}


// ============================================================================
// Process Boot Keyboard report
// ============================================================================
//
// Standard USB HID Boot Keyboard report:
//
// byte 0 : modifiers
// byte 1 : reserved
// byte 2 : key 1
// byte 3 : key 2
// byte 4 : key 3
// byte 5 : key 4
// byte 6 : key 5
// byte 7 : key 6
//
// IMPORTANT:
// Do NOT treat byte 0 as Report ID.
//

static void process_keyboard_report(const uint8_t *data, size_t length)
{
    if (data == NULL)
        return;

    if (length < 8)
        return;


    uint32_t state = 0;


    // ------------------------------------------------------------
    // Byte 0 = modifier bits.
    //
    // Currently not mapped.
    //
    // If later needed:
    //
    // bit 0 = Left Ctrl
    // bit 1 = Left Shift
    // bit 2 = Left Alt
    // bit 3 = Left GUI
    // bit 4 = Right Ctrl
    // bit 5 = Right Shift
    // bit 6 = Right Alt
    // bit 7 = Right GUI
    // ------------------------------------------------------------

    // uint8_t modifiers = data[0];


    // ------------------------------------------------------------
    // Bytes 2..7 = six simultaneous keys
    // ------------------------------------------------------------

    for (int i = 2; i < 8; i++)
    {
        uint8_t code = data[i];

        if (code == 0)
            continue;

        uint32_t key = hid_to_rg_key(code);

        state |= key;
    }


    usb_keyboard_state = state;
}


// ============================================================================
// HID interface callback
// ============================================================================
//
// This callback belongs to an opened HID interface.
//
// INPUT_REPORT:
//     read report.
//
// DISCONNECTED:
//     close interface.
//
// No device OPEN/START here.
//

static void hid_interface_cb(
    hid_host_device_handle_t handle,
    const hid_host_interface_event_t event,
    void *arg)
{
    (void)arg;


    switch (event)
    {
        // ------------------------------------------------------------
        // Input report
        // ------------------------------------------------------------

        case HID_HOST_INTERFACE_EVENT_INPUT_REPORT:
        {
            uint8_t data[64];
            size_t data_len = 0;


            esp_err_t err =
                hid_host_device_get_raw_input_report_data(
                    handle,
                    data,
                    sizeof(data),
                    &data_len
                );


            if (err == ESP_OK)
            {
                process_keyboard_report(data, data_len);
            }
            else
            {
                RG_LOGI(
                    "USB_KBD: get input report failed: %s",
                    esp_err_to_name(err)
                );
            }

            break;
        }


        // ------------------------------------------------------------
        // Device/interface disconnected
        // ------------------------------------------------------------

        case HID_HOST_INTERFACE_EVENT_DISCONNECTED:
        {
            RG_LOGI("USB_KBD: keyboard disconnected");


            usb_keyboard_state = 0;
            usb_keyboard_connected = false;


            esp_err_t err = hid_host_device_close(handle);


            if (err != ESP_OK)
            {
                RG_LOGI(
                    "USB_KBD: hid_host_device_close() = %s",
                    esp_err_to_name(err)
                );
            }


            break;
        }


        default:
            break;
    }
}


// ============================================================================
// Open and start keyboard
// ============================================================================

static void open_keyboard(
    hid_host_device_handle_t handle)
{
    hid_host_dev_params_t params;


    // ------------------------------------------------------------
    // Get HID interface parameters
    // ------------------------------------------------------------

    esp_err_t err =
        hid_host_device_get_params(handle, &params);


    if (err != ESP_OK)
    {
        RG_LOGI(
            "USB_KBD: get_params failed: %s",
            esp_err_to_name(err)
        );

        return;
    }


    RG_LOGI(
        "USB_KBD: device connected - proto=%d, subclass=%d, iface=%d",
        params.proto,
        params.sub_class,
        params.iface_num
    );


    // ------------------------------------------------------------
    // IMPORTANT:
    //
    // Accept ONLY Boot Keyboard.
    //
    // proto=1 = Keyboard
    // proto=2 = Mouse
    // proto=0 = None / other HID interface
    // ------------------------------------------------------------

    if (params.sub_class != HID_SUBCLASS_BOOT_INTERFACE ||
        params.proto != HID_PROTOCOL_KEYBOARD)
    {
        RG_LOGI(
            "USB_KBD: ignoring non-keyboard HID interface"
        );

        return;
    }


    // ------------------------------------------------------------
    // Device configuration
    // ------------------------------------------------------------

    const hid_host_device_config_t dev_cfg =
    {
        .callback = hid_interface_cb,
        .callback_arg = NULL,
    };


    // ------------------------------------------------------------
    // Open
    // ------------------------------------------------------------

    err = hid_host_device_open(handle, &dev_cfg);


    RG_LOGI(
        "USB_KBD: hid_host_device_open() = %s",
        esp_err_to_name(err)
    );


    if (err != ESP_OK)
    {
        return;
    }


    // ------------------------------------------------------------
    // Set Boot Protocol
    // ------------------------------------------------------------

    err =
        hid_class_request_set_protocol(
            handle,
            HID_REPORT_PROTOCOL_BOOT
        );


    RG_LOGI(
        "USB_KBD: set boot protocol = %s",
        esp_err_to_name(err)
    );


    if (err != ESP_OK)
    {
        RG_LOGI(
            "USB_KBD: set protocol failed, closing device"
        );

        hid_host_device_close(handle);

        return;
    }


    // ------------------------------------------------------------
    // Set Idle
    // ------------------------------------------------------------

    err =
        hid_class_request_set_idle(
            handle,
            0,
            0
        );


    RG_LOGI(
        "USB_KBD: set idle = %s",
        esp_err_to_name(err)
    );


    if (err != ESP_OK)
    {
        RG_LOGI(
            "USB_KBD: set idle failed, closing device"
        );

        hid_host_device_close(handle);

        return;
    }


    // ------------------------------------------------------------
    // Start
    // ------------------------------------------------------------

    err = hid_host_device_start(handle);


    RG_LOGI(
        "USB_KBD: hid_host_device_start() = %s",
        esp_err_to_name(err)
    );


    if (err != ESP_OK)
    {
        RG_LOGI(
            "USB_KBD: start failed, closing device"
        );

        hid_host_device_close(handle);

        return;
    }


    usb_keyboard_connected = true;


    RG_LOGI(
        "USB_KBD: keyboard started successfully"
    );
}


// ============================================================================
// HID driver callback
// ============================================================================
//
// IMPORTANT:
//
// This callback MUST NOT perform:
//
//     hid_host_device_open()
//     hid_class_request_set_protocol()
//     hid_class_request_set_idle()
//     hid_host_device_start()
//
// It only queues the event.
//
// This avoids doing HID control transfers from the HID driver's
// own event-processing context.
//

static void hid_driver_cb(
    hid_host_device_handle_t handle,
    const hid_host_driver_event_t event,
    void *arg)
{
    (void)arg;


    RG_LOGI(
        "USB_KBD: HID callback event = %d",
        (int)event
    );


    if (usb_hid_event_queue == NULL)
        return;


    usb_hid_event_t msg =
    {
        .handle = handle,
        .event = event,
    };


    BaseType_t result =
        xQueueSend(
            usb_hid_event_queue,
            &msg,
            0
        );


    if (result != pdPASS)
    {
        RG_LOGI(
            "USB_KBD: HID event queue full"
        );
    }
}


// ============================================================================
// HID application task
// ============================================================================
//
// All HID device control operations happen here.
//

static void usb_hid_event_task(void *arg)
{
    (void)arg;


    RG_LOGI(
        "USB_KBD: HID event task started"
    );


    usb_hid_event_t msg;


    while (usb_keyboard_ready)
    {
        if (xQueueReceive(
                usb_hid_event_queue,
                &msg,
                pdMS_TO_TICKS(1000)) != pdPASS)
        {
            continue;
        }


        // ------------------------------------------------------------
        // CONNECTED
        // ------------------------------------------------------------

        if (msg.event == HID_HOST_DRIVER_EVENT_CONNECTED)
        {
            open_keyboard(msg.handle);
        }
    }


    RG_LOGI(
        "USB_KBD: HID event task stopped"
    );


    usb_hid_task_handle = NULL;

    vTaskDelete(NULL);
}


// ============================================================================
// USB Host daemon task
// ============================================================================
//
// This task services the USB Host library.
//
// Because hid_host_install() uses create_background_task=true,
// we do NOT call hid_host_handle_events() here.
//
// This task only services the USB Host library itself.
//

static void usb_host_task(void *arg)
{
    (void)arg;


    RG_LOGI(
        "USB_KBD: host daemon task started"
    );


    while (usb_keyboard_ready)
    {
        uint32_t event_flags = 0;


        esp_err_t err =
            usb_host_lib_handle_events(
                pdMS_TO_TICKS(1000),
                &event_flags
            );


        if (err != ESP_OK &&
            err != ESP_ERR_TIMEOUT)
        {
            RG_LOGI(
                "USB_KBD: usb_host_lib_handle_events() = %s",
                esp_err_to_name(err)
            );
        }


        // ------------------------------------------------------------
        // NO_CLIENTS
        //
        // Normally not expected while application is running,
        // because HID host client is installed.
        // ------------------------------------------------------------

        if (event_flags &
            USB_HOST_LIB_EVENT_FLAGS_NO_CLIENTS)
        {
            RG_LOGI(
                "USB_KBD: USB_HOST_LIB_EVENT_FLAGS_NO_CLIENTS"
            );
        }


        // ------------------------------------------------------------
        // ALL_FREE
        //
        // Useful during shutdown.
        // ------------------------------------------------------------

        if (event_flags &
            USB_HOST_LIB_EVENT_FLAGS_ALL_FREE)
        {
            RG_LOGI(
                "USB_KBD: USB_HOST_LIB_EVENT_FLAGS_ALL_FREE"
            );
        }
    }


    RG_LOGI(
        "USB_KBD: host daemon task stopped"
    );


    usb_host_task_handle = NULL;

    vTaskDelete(NULL);
}


// ============================================================================
// USB monitor
// ============================================================================

static void usb_monitor_task(void *arg)
{
    (void)arg;


    // Give USB host some time to initialize.
    vTaskDelay(pdMS_TO_TICKS(2000));


    while (usb_keyboard_ready)
    {
        usb_host_lib_info_t info;


        esp_err_t err =
            usb_host_lib_info(&info);


        if (err == ESP_OK)
        {
            RG_LOGI(
                "USB_MON: devices=%d, clients=%d",
                info.num_devices,
                info.num_clients
            );
        }
        else
        {
            RG_LOGI(
                "USB_MON: usb_host_lib_info() = %s",
                esp_err_to_name(err)
            );
        }


        vTaskDelay(pdMS_TO_TICKS(5000));
    }


    usb_monitor_task_handle = NULL;

    vTaskDelete(NULL);
}


// ============================================================================
// Initialize USB Keyboard Host
// ============================================================================

bool rg_usb_keyboard_init(void)
{
    if (usb_keyboard_ready)
    {
        return true;
    }


    RG_LOGI(
        "USB_KBD: initializing USB Host"
    );


    // ------------------------------------------------------------
    // Create event queue first.
    // ------------------------------------------------------------

    usb_hid_event_queue =
        xQueueCreate(
            8,
            sizeof(usb_hid_event_t)
        );


    if (usb_hid_event_queue == NULL)
    {
        RG_LOGI(
            "USB_KBD: failed to create HID event queue"
        );

        return false;
    }


    // ------------------------------------------------------------
    // Install USB Host
    // ------------------------------------------------------------

    usb_host_config_t host_cfg =
    {
        .skip_phy_setup = false,
        .intr_flags = ESP_INTR_FLAG_LEVEL1,
    };


    esp_err_t err =
        usb_host_install(&host_cfg);


    if (err != ESP_OK)
    {
        RG_LOGI(
            "USB_KBD: usb_host_install() = %s",
            esp_err_to_name(err)
        );


        vQueueDelete(usb_hid_event_queue);
        usb_hid_event_queue = NULL;


        return false;
    }


    usb_keyboard_ready = true;


    // ------------------------------------------------------------
    // USB Host daemon task
    // ------------------------------------------------------------

    BaseType_t task_ok =
        xTaskCreatePinnedToCore(
            usb_host_task,
            "usb_host",
            4096,
            NULL,
            2,
            &usb_host_task_handle,
            0
        );


    if (task_ok != pdPASS)
    {
        RG_LOGI(
            "USB_KBD: failed to create USB host task"
        );


        usb_keyboard_ready = false;

        usb_host_uninstall();

        vQueueDelete(usb_hid_event_queue);
        usb_hid_event_queue = NULL;


        return false;
    }


    // ------------------------------------------------------------
    // HID Host driver
    //
    // create_background_task=true:
    //
    // HID driver owns its internal event-processing task.
    // ------------------------------------------------------------

    hid_host_driver_config_t hid_cfg =
    {
        .create_background_task = true,
        .task_priority = 5,
        .stack_size = 4096,
        .core_id = 0,
        .callback = hid_driver_cb,
        .callback_arg = NULL,
    };


    err =
        hid_host_install(&hid_cfg);


    if (err != ESP_OK)
    {
        RG_LOGI(
            "USB_KBD: hid_host_install() = %s",
            esp_err_to_name(err)
        );


        usb_keyboard_ready = false;


        // Give host task a chance to leave its loop.
        vTaskDelay(pdMS_TO_TICKS(20));


        usb_host_uninstall();


        vQueueDelete(usb_hid_event_queue);
        usb_hid_event_queue = NULL;


        return false;
    }


    // ------------------------------------------------------------
    // Our HID application task
    // ------------------------------------------------------------

    task_ok =
        xTaskCreate(
            usb_hid_event_task,
            "usb_hid_evt",
            4096,
            NULL,
            4,
            &usb_hid_task_handle
        );


    if (task_ok != pdPASS)
    {
        RG_LOGI(
            "USB_KBD: failed to create HID event task"
        );


        usb_keyboard_ready = false;


        vTaskDelay(pdMS_TO_TICKS(20));


        hid_host_uninstall();
        usb_host_uninstall();


        vQueueDelete(usb_hid_event_queue);
        usb_hid_event_queue = NULL;


        return false;
    }


    // ------------------------------------------------------------
    // Monitor task
    // ------------------------------------------------------------

    xTaskCreate(
        usb_monitor_task,
        "usb_mon",
        3072,
        NULL,
        4,
        &usb_monitor_task_handle
    );


    RG_LOGI(
        "USB_KBD: USB Host initialized successfully"
    );


    return true;
}

// ============================================================================
// Get current USB keyboard state
// ============================================================================

uint32_t rg_usb_keyboard_get_state(void)
{
    return usb_keyboard_state;
}

// ============================================================================
// Deinitialize
// ============================================================================
//
// IMPORTANT:
//
// This function is intended for complete application shutdown,
// not for normal keyboard reconnects.
//
// Do not call it merely because the keyboard was unplugged.
// The HID/USB stack handles disconnect automatically.
//

void rg_usb_keyboard_deinit(void)
{
    if (!usb_keyboard_ready)
    {
        return;
    }


    RG_LOGI(
        "USB_KBD: deinitializing USB Host"
    );


    usb_keyboard_connected = false;
    usb_keyboard_state = 0;


    // ------------------------------------------------------------
    // Stop our application tasks.
    // ------------------------------------------------------------

    usb_keyboard_ready = false;


    // ------------------------------------------------------------
    // Give tasks a chance to exit their loops.
    // ------------------------------------------------------------

    vTaskDelay(pdMS_TO_TICKS(50));


    // ------------------------------------------------------------
    // HID host must be uninstalled before USB host.
    //
    // If a HID interface is still open, hid_host_uninstall()
    // can return ESP_ERR_INVALID_STATE.
    //
    // Normal runtime does NOT call this function, so this path
    // is only for complete application shutdown.
    // ------------------------------------------------------------

    esp_err_t err =
        hid_host_uninstall();


    if (err != ESP_OK)
    {
        RG_LOGI(
            "USB_KBD: hid_host_uninstall() = %s",
            esp_err_to_name(err)
        );
    }


    // ------------------------------------------------------------
    // USB host
    // ------------------------------------------------------------

    err =
        usb_host_uninstall();


    if (err != ESP_OK)
    {
        RG_LOGI(
            "USB_KBD: usb_host_uninstall() = %s",
            esp_err_to_name(err)
        );
    }


    // ------------------------------------------------------------
    // Queue
    // ------------------------------------------------------------

    if (usb_hid_event_queue != NULL)
    {
        vQueueDelete(usb_hid_event_queue);
        usb_hid_event_queue = NULL;
    }


    usb_keyboard_state = 0;
    usb_keyboard_connected = false;


    usb_host_task_handle = NULL;
    usb_hid_task_handle = NULL;
    usb_monitor_task_handle = NULL;


    RG_LOGI(
        "USB_KBD: USB Host deinitialized"
    );
}