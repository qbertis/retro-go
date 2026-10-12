# USB HID Keyboard Support

## Architecture
USB keyboard
↓
USB OTG controller (GPIO19/20)
↓
USB Host Library
↓
HID Host Driver
↓
usb_hid_event_task (queue-based)
↓
hid_to_rg_key()
↓
RG_KEY_* state

## Key mapping

| Key | RG_KEY |
|---|---|
| Arrows / WASD | D-pad |
| Z / C | A |
| X / V | B |
| Enter / Space | START |
| Esc / Backspace | SELECT |
| M / F1 | MENU |
| O / F2 | OPTION |

## Files

- `components/retro-go/rg_usb_keyboard.c` — main implementation
- `components/retro-go/rg_usb_keyboard.h` — public API
- Integration: `rg_input.c` calls `rg_usb_keyboard_init()` and `rg_usb_keyboard_get_state()`

## Configuration

- Native USB used for OTG (GPIO19/20)
- Console output via UART0 (menuconfig)
- USB-Serial-JTAG disabled