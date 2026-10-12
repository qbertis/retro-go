#pragma once

#include <stdbool.h>
#include <stdint.h>

// Ініціалізація USB Host + HID
bool rg_usb_keyboard_init(void);

// Деініціалізація
void rg_usb_keyboard_deinit(void);

// Поточний стан клавіш у форматі RG_KEY_*
uint32_t rg_usb_keyboard_get_state(void);

// Чи підключена клавіатура зараз
bool rg_usb_keyboard_is_connected(void);