Name:           el1
Summary:        Essentials Library v1
Group:          System Environment/Daemons
Distribution:   openSUSE
License:        GPLv3
URL:            https://www.brennecke-it.net

BuildRequires:  clang, go-md2man, krb5-devel, easy-rpm, postgresql-devel, lld, valgrind-devel
Requires:		krb5-client, libpq5, postgresql, krb5-devel

%description
el1 is a C++ Essentials Library focused on IoT and high-level operations.

%files
%{_includedir}/el1/db.hpp
%{_includedir}/el1/db_postgres.hpp
%{_includedir}/el1/debug.hpp
%{_includedir}/el1/def.hpp
%{_includedir}/el1/dev_dht22.hpp
%{_includedir}/el1/dev_gcode_excellion.hpp
%{_includedir}/el1/dev_gcode_grbl.hpp
%{_includedir}/el1/dev_gpio.hpp
%{_includedir}/el1/dev_gpio_bcm283x.hpp
%{_includedir}/el1/dev_gpio_dcf77.hpp
%{_includedir}/el1/dev_gpio_hd44780.hpp
%{_includedir}/el1/dev_gpio_native.hpp
%{_includedir}/el1/dev_i2c.hpp
%{_includedir}/el1/dev_i2c_ads111x.hpp
%{_includedir}/el1/dev_i2c_mpu6050.hpp
%{_includedir}/el1/dev_i2c_native.hpp
%{_includedir}/el1/dev_i2c_opt3001.hpp
%{_includedir}/el1/dev_i2c_pca9555.hpp
%{_includedir}/el1/dev_spi.hpp
%{_includedir}/el1/dev_spi_hx711.hpp
%{_includedir}/el1/dev_spi_led.hpp
%{_includedir}/el1/dev_spi_led_neopixel.hpp
%{_includedir}/el1/dev_spi_native.hpp
%{_includedir}/el1/dev_spi_w1bb.hpp
%{_includedir}/el1/dev_w1.hpp
%{_includedir}/el1/dev_w1_ds18x20.hpp
%{_includedir}/el1/el1.hpp
%{_includedir}/el1/error.hpp
%{_includedir}/el1/io_bcd.hpp
%{_includedir}/el1/io_collection_list.hpp
%{_includedir}/el1/io_collection_map.hpp
%{_includedir}/el1/io_compression_deflate.hpp
%{_includedir}/el1/io_file.hpp
%{_includedir}/el1/io_format_base64.hpp
%{_includedir}/el1/io_format_json.hpp
%{_includedir}/el1/io_graphics_color.hpp
%{_includedir}/el1/io_graphics_image.hpp
%{_includedir}/el1/io_graphics_image_format_png.hpp
%{_includedir}/el1/io_graphics_image_format_pnm.hpp
%{_includedir}/el1/io_graphics_plotter.hpp
%{_includedir}/el1/io_net_eth.hpp
%{_includedir}/el1/io_net_http.hpp
%{_includedir}/el1/io_net_ip.hpp
%{_includedir}/el1/io_serialization.hpp
%{_includedir}/el1/io_stream.hpp
%{_includedir}/el1/io_stream_buffer.hpp
%{_includedir}/el1/io_stream_fifo.hpp
%{_includedir}/el1/io_stream_producer.hpp
%{_includedir}/el1/io_text.hpp
%{_includedir}/el1/io_text_encoding.hpp
%{_includedir}/el1/io_text_encoding_utf8.hpp
%{_includedir}/el1/io_text_parser.hpp
%{_includedir}/el1/io_text_string.hpp
%{_includedir}/el1/io_text_terminal.hpp
%{_includedir}/el1/io_types.hpp
%{_includedir}/el1/math.hpp
%{_includedir}/el1/math_matrix.hpp
%{_includedir}/el1/math_polygon.hpp
%{_includedir}/el1/math_vector.hpp
%{_includedir}/el1/security_kerberos.hpp
%{_includedir}/el1/system_cmdline.hpp
%{_includedir}/el1/system_handle.hpp
%{_includedir}/el1/system_memory.hpp
%{_includedir}/el1/system_random.hpp
%{_includedir}/el1/system_task.hpp
%{_includedir}/el1/system_time.hpp
%{_includedir}/el1/system_time_timer.hpp
%{_includedir}/el1/system_waitable.hpp
%{_includedir}/el1/util.hpp
%{_includedir}/el1/util_bits.hpp
%{_includedir}/el1/util_event.hpp
%{_includedir}/el1/util_function.hpp
%{_libdir}/libel1.so

%prep
%setup -q -n %{name}

%build

%install
make install %{?_smp_mflags} VERSION="%{version}" INCLUDE_DIR="%{buildroot}%{_includedir}" LIB_DIR="%{buildroot}%{_libdir}"

%changelog
