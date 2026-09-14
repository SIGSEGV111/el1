# Reference material

Files in this directory are reference material and are not inputs to the normal
el1 build.

## `hx711.png`

Hardware/reference image retained for HX711 development. It is not consumed by
the HX711 implementation or build system.

## HD44780 character map

The HD44780 character-map data used by el1 is checked in as C++ source. The old
JavaScript source/generator files were historical generation aids and have been
removed; regenerating `dev_gpio_hd44780.data.cpp` is no longer part of the build.
