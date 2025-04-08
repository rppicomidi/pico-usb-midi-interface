# pico-usb-midi-interface
The purpose of this project is to make a computer USB interface
for as many serial port MIDI I/O streams as the chip can support. You can
make an
8-input 8-output USB device to serial port MIDI interface using the Raspberry Pi Pico 2 (or equivalent RP2350 chip board) or a
6-input 6-output USB device to serial port MIDI interface using the Raspberry Pi Pico (or equivalent RP2040 chip board).

This project uses both on-chip hardware UART modules to create two of the serial port MIDI I/O ports.
The remaining 6 MIDI I/O ports for the Pico 2 or 4 MIDI I/O ports for the Pico are built using PIO state machines.
Each PIO MIDI serial port consumes 2 state machines, so each on-chip PIO module can support 2 MIDI serial I/O ports.
The Pico 2 has 3 PIO modules, so it can support 6 PIO MIDI I/O. The Pico has 2 PIO modules, so it can support 4 PIO MIDI I/O.

# Hardware                                                   
This project requires upt to 8 MIDI IN and 8 MIDI OUT serial port interfaces. The simplest way
to get the interface hardware is to buy it off the shelf. I used the
[Adafruit MIDI FeatherWing](https://www.adafruit.com/product/4740) to
test one port at a time on a breadboard. The 3.3V supply from the Pico
board is more than powerful enough to power the board.

You could buy 8 of these to implement this project, or could hand-build
the [circuit](https://learn.adafruit.com/assets/95273) 8 times.
The advantage of hand-building is you can choose what connectors you
want to use and you can choose whether or not to install LEDs
for MIDI IN and MIDI OUT. See the file `main.c` and the datasheet
for your RP2040 or RP2350 board for mapping the board pins to each
transmit and receive pin.

On the referenced schematic, the MIDI_TX
pin goes to the corresponding MIDI_OUT GPIO pin and the MIDI_RX
pin goes to the corresponding MIDI IN GPIO pin. Note that the
GPIO numbers in the software are not Pico board pin numbers.

I chose the GPIO numbers for this project so that all the serial
port MIDI pins are on one side of the Pico board. If the pins I
chose are not convenient for your board, please chose other pins
and change the software accordingly. If you choose to have LEDs
for each MIDI IN and MIDI OUT connector, please adjust the board power
requirements in the `usb_descriptors.c` definition of
`desc_fs_configuration` according to how much power each LED will draw.
Keep in mind the power consumption of the Pico family board and note
each serial MIDI OUT will draw about 5 mA from the 3.3V power supply.

# Software
This project is designed to build with `pico-sdk` version 2.1.1 or later.
The software I have written is released under the MIT license.
It uses several libraries that are managed as git submodules. See
each library for its own license.

To get the software:
```
git clone --recurse-submodules https://github.com/rppicomidi/pico-usb-midi-interface.git
```
You can build the software on command line in the standard way:
```
export PICO_SDK_PATH=[the path to your pico-sdk]
export PICO_BOARD=[pico|pico_w|pico2|whatever is your board]
cd pico-usb-midi-interface
mkdir build
cd build
cmake ..
make
```
This project should also cleanly import to VS Code using the Official Raspberry Pi Pico
VS Code extension. From there you can build it as usual.

# CLI
In addition to USB MIDI, the USB computer interface also provides Command Line Interpreter (CLI)
user interface via a USB CDC-ACM serial port. On Linux, this interface will appear as
`/dev/ttyACM?`, where `?` is some number. On a PC, this interface will be some `COM` port.

If you launch a serial port console such as `minicom` (Linux or a Mac) or `putty` (on a PC) on the same host computer that is providing the MIDI host interface,
ythen ou can change the USB routing from the defaults. The PIO Serial MIDI ports are labeled A-F and the hardware Serial MIDI ports are labeled G-H.
The USB MIDI ports are numbered 1-8.
By default, USB MIDI port 1 routes to Serial MIDI port A, USB MIDI port 2 routes to Serial MIDI port B, and so on. The last 2 USB ports always
map to the hardware seriall ports G and H. However, you can choose to route any Serial MIDI or USB MIDI IN to any or
all Serial MIDI or USB MIDI OUT ports. For example, you can make MIDI OUT A a MIDI THRU port for MIDI IN A by routing MIDI IN A to MIDI OUT A.
If you route more than one MIDI IN to
a single MIDI out, you will be merging data streams; this can cause bandwidth problems if both MIDI IN streams contain a lot of MIDI data.

The CLI is based on the [embedded-cli](https://github.com/funbiscuit/embedded-cli) project. You can use the arrow keys to edit or recall
previous commands, you can use the backspace and delete keys to edit
commands, and you can use the tab key to autocomplete commands.

Shortly after you connect a serial port terminal to the USB CDC-ACM port
this project generates, You will see a brief welcome message:
```
Cli is running.
Type "help" for a list of commands
Use backspace and tab to remove chars and autocomplete
Use up and down arrows to recall previous commands

The single character port ID to use in commands can be
1-6 for USB MIDI and can be A-D, G-H for Serial MIDI
>
```
The `>` symbol is the command prompt. The following commands are available.

## `help`
If you type the `help` command, you will get a display like this:
```
> help 
 * help
        Print list of commands
 * connect
        Route a MIDI stream. usage connect <From (1-8 or A-H)> <To (1-8 or A-H)>
 * disconnect
        Unroute a MIDI stream. usage disconnect <From (1-8 or A-H)> <To (1-8 or A-H)>
 * show
        Show MIDI stream routing. usage: show
```
## `connect` and `disconnect`
These two commands will let you change routing. The help description
above says it all.

## `show`
To display the current connection setup, type the `show` command. The
USB and serial MIDI data ports that stream into this project are enumerated in rows and the USB and serial MIDI data ports that stream out of this project are enumerated in columns. Data will stream from one port to
another port if there is an 'X' character in the box at the intersection of the corresponding row and column. Otherwise, the box will be blank.
For RP2350 based systems, the default connection matrix looks like this:
```
        TO->|   |   |   |   |   |   |   |   | S | S | S | S | S | S | S | S |
            |   |   |   |   |   |   |   |   | E | E | E | E | E | E | E | E |
            |   |   |   |   |   |   |   |   | R | R | R | R | R | R | R | R |
            | U | U | U | U | U | U | U | U | I | I | I | I | I | I | I | I |
            | S | S | S | S | S | S | S | S | A | A | A | A | A | A | A | A |
            | B | B | B | B | B | B | B | B | L | L | L | L | L | L | L | L |
            |   |   |   |   |   |   |   |   |   |   |   |   |   |   |   |   |
  FROM |    | O | O | O | O | O | O | O | O | O | O | O | O | O | O | O | O |
       v    | U | U | U | U | U | U | U | U | U | U | U | U | U | U | U | U |
            | T | T | T | T | T | T | T | T | T | T | T | T | T | T | T | T |
            |   |   |   |   |   |   |   |   |   |   |   |   |   |   |   |   |
            | 1 | 2 | 3 | 4 | 5 | 6 | 7 | 8 | A | B | C | D | E | F | G | H |
------------+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+
    USB IN 1|   |   |   |   |   |   |   |   | X |   |   |   |   |   |   |   |
------------+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+
    USB IN 2|   |   |   |   |   |   |   |   |   | X |   |   |   |   |   |   |
------------+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+
    USB IN 3|   |   |   |   |   |   |   |   |   |   | X |   |   |   |   |   |
------------+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+
    USB IN 4|   |   |   |   |   |   |   |   |   |   |   | X |   |   |   |   |
------------+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+
    USB IN 5|   |   |   |   |   |   |   |   |   |   |   |   | X |   |   |   |
------------+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+
    USB IN 6|   |   |   |   |   |   |   |   |   |   |   |   |   | X |   |   |
------------+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+
    USB IN 7|   |   |   |   |   |   |   |   |   |   |   |   |   |   | X |   |
------------+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+
    USB IN 8|   |   |   |   |   |   |   |   |   |   |   |   |   |   |   | X |
------------+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+
 SERIAL IN A| X |   |   |   |   |   |   |   |   |   |   |   |   |   |   |   |
------------+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+
 SERIAL IN B|   | X |   |   |   |   |   |   |   |   |   |   |   |   |   |   |
------------+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+
 SERIAL IN C|   |   | X |   |   |   |   |   |   |   |   |   |   |   |   |   |
------------+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+
 SERIAL IN D|   |   |   | X |   |   |   |   |   |   |   |   |   |   |   |   |
------------+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+
 SERIAL IN E|   |   |   |   | X |   |   |   |   |   |   |   |   |   |   |   |
------------+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+
 SERIAL IN F|   |   |   |   |   | X |   |   |   |   |   |   |   |   |   |   |
------------+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+
 SERIAL IN G|   |   |   |   |   |   | X |   |   |   |   |   |   |   |   |   |
------------+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+
 SERIAL IN H|   |   |   |   |   |   |   | X |   |   |   |   |   |   |   |   |
------------+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+
```

For RP2040 based systems, the default connection matrix looks like this:
```
        TO->|   |   |   |   |   |   | S | S | S | S | S | S |
            |   |   |   |   |   |   | E | E | E | E | E | E |
            |   |   |   |   |   |   | R | R | R | R | R | R |
            | U | U | U | U | U | U | I | I | I | I | I | I |
            | S | S | S | S | S | S | A | A | A | A | A | A |
            | B | B | B | B | B | B | L | L | L | L | L | L |
            |   |   |   |   |   |   |   |   |   |   |   |   |
  FROM |    | O | O | O | O | O | O | O | O | O | O | O | O |
       v    | U | U | U | U | U | U | U | U | U | U | U | U |
            | T | T | T | T | T | T | T | T | T | T | T | T |
            |   |   |   |   |   |   |   |   |   |   |   |   |
            | 1 | 2 | 3 | 4 | 5 | 6 | A | B | C | D | G | H |
------------+---+---+---+---+---+---+---+---+---+---+---+---+
    USB IN 1|   |   |   |   |   |   | X |   |   |   |   |   |
------------+---+---+---+---+---+---+---+---+---+---+---+---+
    USB IN 2|   |   |   |   |   |   |   | X |   |   |   |   |
------------+---+---+---+---+---+---+---+---+---+---+---+---+
    USB IN 3|   |   |   |   |   |   |   |   | X |   |   |   |
------------+---+---+---+---+---+---+---+---+---+---+---+---+
    USB IN 4|   |   |   |   |   |   |   |   |   | X |   |   |
------------+---+---+---+---+---+---+---+---+---+---+---+---+
    USB IN 5|   |   |   |   |   |   |   |   |   |   | X |   |
------------+---+---+---+---+---+---+---+---+---+---+---+---+
    USB IN 6|   |   |   |   |   |   |   |   |   |   |   | X |
------------+---+---+---+---+---+---+---+---+---+---+---+---+
 SERIAL IN A| X |   |   |   |   |   |   |   |   |   |   |   |
------------+---+---+---+---+---+---+---+---+---+---+---+---+
 SERIAL IN B|   | X |   |   |   |   |   |   |   |   |   |   |
------------+---+---+---+---+---+---+---+---+---+---+---+---+
 SERIAL IN C|   |   | X |   |   |   |   |   |   |   |   |   |
------------+---+---+---+---+---+---+---+---+---+---+---+---+
 SERIAL IN D|   |   |   | X |   |   |   |   |   |   |   |   |
------------+---+---+---+---+---+---+---+---+---+---+---+---+
 SERIAL IN G|   |   |   |   | X |   |   |   |   |   |   |   |
------------+---+---+---+---+---+---+---+---+---+---+---+---+
 SERIAL IN H|   |   |   |   |   | X |   |   |   |   |   |   |
------------+---+---+---+---+---+---+---+---+---+---+---+---+
```
Note for the RP2040 system that there is no E or F serial MIDI port
because RP2040 only supports 4 PIO serial MIDI ports. There are
also only 6 USB MIDI ports to allow 1:1 routing of USB to serial
MIDI stream mapping.

# Future features
Possible future features on my radar include
- Ability to save and recall routing presets
- Processing MIDI signals between input and output
- A 2 IN and 2 OUT variation that does not use PIO ports
- A 4 IN and 4 OUT variation that does not use HW UART ports

