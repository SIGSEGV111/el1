# el1 examples

Build the current library and every example from the repository root:

```sh
make examples
```

Run the hardware-independent smoke tests and the command-line parser of every hardware example:

```sh
make examples-test
```

Binaries are written to `gen/<arch>/examples/` and use the in-tree `gen/<arch>/release/libel1.so` through a relative runtime search path.

## Hardware examples

- `ads111x`: ADS111x ADC over I2C, optionally using a GPIO data-ready interrupt.
- `dcf77-gpio`: DCF77 receiver on a Linux GPIO character-device line, optionally exporting time through Chrony SHM.
- `gpio-blink`: output toggling or input pull-resistor testing through `/dev/gpiochip*`.
- `gpio-trigger`: edge event monitoring through `/dev/gpiochip*` with optional hardware debounce.
- `hx711-test`: HX711 sampling through SPI, optionally using GPIO chip-enable and interrupt lines.
- `neopixel-spi-driver`: WS2812B HTTP color endpoint through SPI.
- `w1-test`: DS18B20/DS18S20 scan and temperature readout through either SPI bit-banging or a DS2482S-100.

Use `--help` on each binary for device paths and parameters. Hardware examples require the corresponding device nodes and permissions.

### DS2482S-100 example

```sh
gen/"$(rpm --eval '%{_target_cpu}')"/examples/w1-test \
	--backend=ds2482 \
	--i2c-device=/dev/i2c-1 \
	--address=24 \
	--active-pullup=true \
	--power=auto
```

### SPI 1-Wire example

```sh
gen/"$(rpm --eval '%{_target_cpu}')"/examples/w1-test \
	--backend=spi \
	--spi-device=/dev/spidev0.0 \
	--invert-tx=true \
	--min-transfer-bytes=96 \
	--power=dedicated
```
