# BLE Keyboard using ESP32

This code instructs 2 halves of the [Totem keyboard](https://github.com/GEIGEIGEIST/TOTEM) running XIAO ESP32C3 chips to communicate with a third XIAO ESP32C3 chip that acts like a dongle, receiving the pressed keys of each half and managing layers and HID reports.

Ideally this dongle would send the HID reports via USB, but the ESP32C3 does not support USB (it has a USB-C entry but only for power and serial). So the solution was to create a BLE HID device, which has limitations (slower communication, won't connect to PC during BIOS). 

## How to run

If using nix, run `nix-shell` to land on a development environment. If not, open `shell.nix` and install the dependencies mentioned there (Arduino CLI, Python 3.11 and the pyserial lib).

Install the esp32 arduino library by running `arduino-cli core install esp32:esp32`.

Compile the halves with `arduino-cli compile --fqbn esp32:esp32:XIAO_ESP32C3 TotemLeft` and `arduino-cli compile --fqbn esp32:esp32:XIAO_ESP32C3 TotemRight`, and the dongle with `arduino-cli compile --fqbn esp32:esp32:XIAO_ESP32C3 TotemDongle`.

Connect the devices to the computer with USB and run `arduino-cli upload --fqbn esp32:esp32:XIAO_ESP32C3 -p <SERIAL_PORT> TotemLeft/TotemRight/TotemDongle`. You can find the port of the device by running `arduino-cli board list`. Usually the ones connected first will have the smaller numbers at the end.

Search your host device for a Bluetooth device called `Totem Dongle`, connect to it and start typing. If it doesn't work as expected, use `arduino-cli monitor -p <SERIAL_PORT>` to see the serial logs and understand if the issue is in the halves or in the dongle.

## Warning

The dongle connects to the halves via Bluetooth when it identifies that they have the desired SERVICE_UUID and CHARACTERISTIC_UUID. This is used to avoid connections with irrelevant devices nearby, but it's not a security feature. Nothing currently stops an attacker from connecting to the dongle pretending to be the halves and start sending keystrokes to the host computer. Be careful and immediately switch off your dongle if you notice unwanted keyboard behavior.

This type of interference could also happen if you are nearby another person who's using this very firmware, so it's recommended to change the default SERVICE_UUID and CHARACTERISTIC_UUID from TotemLeft, TotemRight and TotemDongle.

You should be safe as long as both halves are connected to the dongle (it stops looking for them) and your dongle is connected to your desired device. Since the dongle only connects to one device, there is no risk of an attacker connecting to the dongle and keylogging you. If you press keys and don't see the expected changes, solve it before typing sensitive info like passwords (this advice is valid for all Bluetooth keyboards).

## How to customize

You probably don't want to customize `TotemLeft.ino` or `TotemRight.ino`. They simply apply the keyboard matrix (sends signals to columns and checks what rows received) and sends a integer to the dongle that is a bitmask of the state of the 18 keys of the half (pressed is 1, released is 0). If you monitor the serial logs for the half, you should see numbers appearing whenever a key is pressed or released. The number is base10 so pressing a single key should return 2 to the power of that key. You should change the UUIDs for service and characteristic (create random ones) and you may want to change the delay of the keyboard matrix.

You also probably don't want to customize `TotemDongle.ino`. If handles the Bluetooth connections between the dongle and the two halves and also with the host machine. You should change the UUIDs to match the ones in the halves of the keyboard if you changed them in `TotemLeft.ino` and `TotemRight.ino`.

The file that you're looking for is `Keyboard.ino`. This will contain the logic that checks for the changes of state in the halves (keys pressed or released) and creates the HID reports to send to the host device (i.e. behave as a keyboard). You can notice that this is low-level compared to ZMK or QMK where you simply create a file describing your preferences and the firmware adapts to this input. This approach is more difficult but it's also simpler in my opinion. Layers are simply an integer, and when a key is pressed or released, the current value of this integer changes the HID report. If a key is dedicated to change between layers, pressing or releasing it will not send any HID reports, unlike traditional keyboards where pretty much all keys send information to the host. The hold-tap behavior is also simple, you save the time of press in a variable, and if the key is released within x ms, apply the consequences of a tap. If not, store a value in memory so that other keys can react accordingly knowing that that key is being held (useful for temporary layers and Homerow mods).

Have fun and feel free to raise issues and open PRs with improvements.
