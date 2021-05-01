# harmonizeRC
Use Harmony remote to send IR data from RaspberryPi

Please note that newer versions of Logitech's Unifying dongle are incompatible
with Harmony remotes. They can't report the directional keys, the menu key, and
the number keys. Old firmware seems to work fine, though. I have successfully
received all keys on the Harmony remote, using firmware version RQR12.03_B0025
for the Unifying dongle. This dongle uses a Nordic chipset; I don't know whether
TI chipsets work as well. Unfortunately, firmware generally can't be downgraded,
so you will likely have to hunt down old stock hardware.
