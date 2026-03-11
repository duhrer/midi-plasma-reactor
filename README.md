# MIDI Plasma Reactor

This project configures a Raspberry Pi Pico (2) to respond to MIDI messages by
updating a string of
[NeoPixel](https://learn.adafruit.com/adafruit-neopixel-uberguide/the-magic-of-neopixels)
lights.

When MIDI messages are received, the lights are updated to reflect the pitch and
velocity (loudness) of all playing notes.  You must use this with something like
a MIDI router that is configured to send it MIDI note messages (see below for details).

This project was created based on [a template provided by the Lichen
community](https://github.com/lichen-community-systems/pi-pico-project-template),
see that project for more background and additional details.

## Hardware Prerequisites

In order to use this project, you will need:

1. A Raspberry Pi Pico (or Pico 2).
2. A string of Neopixel Lights
3. A MIDI controller (or software equivalent).

I use this project with a [Pimoroni Plasma
2350](https://shop.pimoroni.com/products/plasma-2350?variant=42092628246611). It
includes a boost converter to handle the higher power requirements of large
strings of lights, and has a screw terminal you can use to connect/disconnect
many commercial lights without soldering.

I use [this 10 m string of 66
lights](https://shop.pimoroni.com/products/10m-addressable-rgb-led-star-wire?variant=41375620530259)
that comes with the starter kit Pimoroni sells, but you should be able to use it
with premade "curtains" like [this
unit](https://nl.aliexpress.com/item/1005010382840396.html).

## Software Prerequisites

1. All build prerequisites (see below).
2. MIDI routing software such as [midiconn](https://mfeproject.itch.io/midiconn)
   (Windows, Linux) or [MIDI patchbay](https://github.com/notahat/midi_patchbay)
   (OS X).
3. (Optional) Software that produces MIDI messages (such as a sequencer or virtual
   keyboard).

## Build Software Prerequisites

In order to build this project, you will need the following software:

1. Git
2. CMake
3. Docker and/or the Pi Pico SDK toolchain

### Install OpenOCD, Picotool, and the Compiler Toolchain

Raspberry Pi has provided a [Getting Started with Raspberry Pi Pico-series](https://pip-assets.raspberrypi.com/categories/610-raspberry-pi-pico/documents/RP-008276-DS-1-getting-started-with-pico.pdf) PDF that walks through the process of installing the necessary dependencies manually.

#### Recommended: Use the Raspberry Pi Pico VS Code Extension

If you use VS Code, there is a Raspberry Pi Pico extension that will automatically install or update the Pico SDK, toolchain, OpenOCD, picotool, CMake, and other necessary dependencies in ```~/.pico-sdk```.

1. Open VS Code
2. Install the official "Raspberry Pi Pico" extension from Raspberry Pi.
3. Open the Raspberry Pi Pico Project: Quick Access panel
4. Generate a new C/C++Pico project, which will cause the extension to install all necessary dependencies
5. Delete the project
6. Set your environment variables to point to the correct versions of the SDK, OpenOCD, and the ARM toolchain as described below.

#### Other Ways to Install the Build Prerequisites

##### Windows

Nikhil Dabas provides a [Windows GUI installer for the Pico toolchain](https://www.raspberrypi.com/news/raspberry-pi-pico-windows-installer/).

##### Raspberry Pi OS

Raspberry Pi provides a [setup script for Raspberry Pi OS-based Linux](https://github.com/raspberrypi/pico-setup).

## Building the Project

When you have the required build software installed, you can build the project
as follows:

1. Check out the code of this project (including all submodules).
2. Set the required environment variables.
3. Configure the board and processor type.
4. Build the binary.
5. Install the project on your microcontroller.

### Check Out the Code

You will need to checkout out this project's code from [the repository on
GitHub](https://github.com/duhrer/midi-plasma-reactor).  This project use
submodules, so you need to pass a flag when checkout out (or updating) your copy
of the code.  To check out this project's code and all of the required
submodules, use a command like:

```git checkout --recurse-submodules https://github.com/duhrer/midi-plasma-reactor```

If you already have the code checked out and want to update it, you can use
commands like:

```
git pull
git submodule update --recursive
```

### Set Environment Variables

In order to build this project, the following environment variables need to be set:

1. ```PICO_OPENOCD_PATH``` should point to a directory containing an RP2350-compatible version of OpenOCD (which should contain both the openocd binary and its scripts directory). On macOS, the VS Code extension will install this in ```~/.pico-sdk/openocd/<version>```.
2. ```PICO_TOOLCHAIN_PATH``` should point to a directory containing the arm-eabi-none GCC cross-compiler. On macOS, the VS Code extension will install this in ```~/.pico-sdk/toolchain/<version>```.
3. ```picotool_DIR``` should be set if you don't have picotool installed globally (the default if installed by the VS Code extension) and don't want it to be downloaded each time you build the project. On macOS, the VS Code extension will install picotool in ```~/.sdk/picotool/<version>```

#### Example Environment Variables

```sh
PICO_TOOLS_PATH=~/.pico-sdk
export PICO_TOOLCHAIN_PATH=$PICO_TOOLS_PATH/toolchain/13_3_Rel1
export PICO_OPENOCD_PATH=$PICO_TOOLS_PATH/openocd/0.12.0+dev
```

## Configuring the Project

### Configuring the Lights

This project has three key variables that control how the attached lights are
used:

1. `NEOPIXEL_PIN`: The GPIO pin to which the lights are connected.  Defaults to
   `14`, the default for a Pimoroni Plasma 2350.
2. `NEOPIXEL_TOTAL_LIGHTS`: The total number of lights available. Defaults to 65.
3. `NEOPIXEL_NUM_COLUMNS`: The number of neopixel lights per row. Defaults to 13.
4. `NEOPIXEL_NUM_ROWS`: As you may have an uneven number of lights, this
   variable controls the number of rows displayed. Defaults to 5.

### Configuring the Processor and Board

The project name, hardware platform, and device are specified in CMakeLists.txt,
and are set appropriately for a Pico 2 W. They should be customized for your
project:

```CMake
set(NAME blinky)
set(PICO_PLATFORM rp2350)
set(PICO_BOARD pico2_w)
```

You can also override these variables when invoking CMake with
```-DPICO_BOARD=<your board>``` and ```-DPICO_BOARD=<your platform>```

### Choosing between Release and Debug Builds

The CMake build is set up to build optimized Debug versions by default. To build
an unoptimized debug build, include ```-DPICO_DEOPTIMIZED_DEBUG=1``` when you
invoke the compile script (or CMake directly). A Release build can be generated
by specifying ```-DCMAKE_BUILD_TYPE=Release```.

## Compiling

The firmware can be compiled either in a Docker container or using a
locally-installed version of the Pi Pico development toolchain. Flashing the
firmware is be done locally using the Pico fork of OpenOCD.

Support for building and remote debugging the firmware in VS Code is also
included.

### Compiling Locally in the Shell

Assuming the necessary toolchain is installed, a couple of scripts are provided
for compiling locally:

#### Compiling the Firmware

```sh
./compile.sh
```

#### Flashing the Firmware Using the Debug Probe

```sh
./flash-firmware.sh
```

#### Cleaning Up the Build Artifacts

```sh
./clean.sh
```

### Building and Debugging with VS Code

You'll need a Pi Pico Probe connected to your computer, and your Pico board
should also be connected via USB. The project is set up with OpenOCD acting as a
debug server for arm-eabi-none-gdb running on your computer.

Build tasks have been defined for building unoptimized Debug builds and Release
builds.

The CMake extension for VS Code has been set up as well, and can be used to
invoke a build from the Command Palette by first choosing "CMake: Configure" and
then "CMake: Build". See ```.vscode/settings.json``` for the CMake extension's
settings.


### Compiling with Docker

The Docker image contains the Pi Pico cross-compilation toolchain pre-installed,
so you don't need to have it installed locally (unless you want use a debugger).
It assumes you will bind mount the project directory as a volume in the Docker
container.

#### Building the Docker Image

```sh
docker build . -t blinky
```

#### Compiling the Firwmware

```sh
docker run -v `pwd`:/project --rm blinky ./docker-compile.sh
```

#### Running an interactive shell in the container

```sh
docker run -v `pwd`:/project -it --rm blinky
```

## Updating

If you need to update to a newer version of the Pico SDK, here's the process for
updating the submodules:

### Update the pico-sdk Submodule

```sh
cd lib/pico-sdk

# Fetch new tags from the pico-sdk repository
# and check out the version you want to upgrade to.
# After, you'll be in a detached head state.
# That is ok.
git fetch --tags
git checkout <pico-sdk-release-tag-name>

# Update all of Pico SDK's submodules
git submodule update --init --recursive

# Go back to the repository's top level
# and commit the submodule's changes.
cd ../..
git add lib/pico-sdk
git commit
```

### Update the Tags Used in the Dockerfile

The Docker file specifies tag versions for pico-sdk and picotool. These lines
should be updated accordingly:

```dockerfile
RUN git clone https://github.com/raspberrypi/pico-sdk.git --branch 2.2.0

...

RUN git clone https://github.com/raspberrypi/picotool.git --branch 2.2.0-a4
```

## Actually Using the Project

Once you have installed the binary on a microcontroller, connect it to both a
computer and to the lights you wish to control. When you connect power, the unit
will be in "screensaver" mode, in which the lights gently flash with colours.

Using MIDI routing software (see above), connect this unit (MIDI Plasma Reactor)
to your desired MIDI input or passthrough. When you send MIDI note on message
(i.e. play notes), lights will be activated in response to the pitch and
velocity (loudness).

If no MIDI messages are received in five minutes, the unit will resume its
screensaver mode.

## Credits

The template used to create this project was created by Colin Clark of [Lichen
Community Systems Worker Cooperative Canada](https://lichen.coop).

The project itself was created by Tony Atkins, also part of the Lichen
Community.

## License

The code in this repository is distributed under the BSD-3 license.
