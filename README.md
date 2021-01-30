# libsddc - A low level library for wideband SDR receivers

A library and a few simple applications for wideband SDR receivers like BBRF103, RX-666, RX888, HF103, etc

**IMPORTANT: the development of this library has now become part of Oscar's ExtIO_sddc project: https://github.com/ik1xpv/ExtIO_sddc - since the Linux code in that repository is not yet fully functional, I am keeping this version around for now in order to provide an option for Linux users to be able to stream samples from the RX888**


These SDR receivers and this library wouldn't be possible without all the excellent work by Oscar Steila, IK1XPV - a great deal of useful information about them is at <http://www.steila.com/blog/> and at <https://sdr-prototypes.blogspot.com/>.

This library is similar in concept to librtlsdr (see <https://osmocom.org/projects/rtl-sdr/wiki> and <https://github.com/librtlsdr/librtlsdr>).

I wrote this library and the example applications from scratch (i.e. any bug in this code is exclusively my fault). Many parts of the code use Oscar's ExtIO dll driver for Windows (<https://github.com/ik1xpv/ExtIO_sddc>) as a reference, and I want to really thank him for this wonderful project!


NOTICE: this library expects the SDR to be running firmware 1.01 or above; for firmware versions 0.X (0.4, 0.5, etc), please see the 'rf103' project here: https://github.com/fventuri/RF103


The firmware directory contains Oscar Steila's firmware for convenience. The source code for the firmware is here: https://github.com/ik1xpv/ExtIO_sddc/tree/master/SDDC_FX3


## Credits

- Oscar Steila, IK1XPV for the BBRF103 and HF103 projects
- Hayati Aygün for many improvements and bug fixes to the code, adding useful examples, and many useful suggestions
- Jakob Ketterl, DD5JFK for many hours and days spent troubleshooting the code and fixing my bugs, and for his ideas on how to improve the library
- Howard Su and Justin Peng for all their work and experimentation on the RX888 hardware, improving the firmware and teamwork for the Windows ExtIO dll driver
- Takafumi JI3GAB for the latest set of fixes to be able to run using the provided firmware


## How to build

```
cd libsddc
mkdir build
cd build
cmake ..
make
sudo make install
sudo ldconfig
```

## udev rules

On Linux usually only root has full access to the USB devices. In order to be able to run these programs and other programs that use this library as a regular user, you may want to add some exception rules for these USB devices. A simple and effective way to create persistent rules (which will last even after a reboot) is to add the file <misc/99-sddc.rules> to your udev rule directory '/etc/udev/rules.d' and tell 'udev' to reload its rules.

These are the commands that need to be run only once using sudo to grant access to these SDRs to a regular user:
```
sudo cp misc/99-sddc.rules /etc/udev/rules.d/
sudo udevadm control --reload-rules
```

For some distributions (like Linux Mint), you may also need this command (thanks to Jon Fear for finding out):
```
sudo udevadm trigger
```

## Copyright

(C) 2020 Franco Venturi - Licensed under the GNU GPL V3 (see <LICENSE>)

Firmware:
Copyright (c) 2017-2020 Oscar Steila ik1xpv<at>gmail.com
