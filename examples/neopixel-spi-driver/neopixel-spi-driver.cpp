#include <el1/dev_gpio_native.hpp>
#include <el1/dev_spi_led.hpp>
#include <el1/dev_spi_led_neopixel.hpp>
#include <el1/dev_spi_native.hpp>
#include <el1/error.hpp>
#include <el1/io_file.hpp>
#include <el1/io_format_json.hpp>
#include <el1/io_net_http.hpp>
#include <el1/io_net_ip.hpp>
#include <el1/io_text_encoding_utf8.hpp>
#include <el1/system_cmdline.hpp>
#include <el1/system_task.hpp>

#include <cstdio>
#include <memory>

int main(const int argc, char* argv[])
{
	using namespace el1;
	using namespace el1::dev::gpio;
	using namespace el1::dev::gpio::native;
	using namespace el1::dev::spi::led;
	using namespace el1::dev::spi::led::neopixel;
	using namespace el1::dev::spi::native;
	using namespace el1::error;
	using namespace el1::io::file;
	using namespace el1::io::format::json;
	using namespace el1::io::net::http;
	using namespace el1::io::net::ip;
	using namespace el1::io::text::encoding::utf8;
	using namespace el1::system::cmdline;
	using namespace el1::system::task;

	try
	{
		TPath path_spi = "/dev/spidev0.0";
		TPath path_gpio_chip = "/dev/gpiochip0";
		s64_t chip_enable_line = -1;
		s64_t led_count = 1;
		s64_t http_port = 8080;

		ParseCmdlineArguments(argc, argv,
			THelpArgument(U"Control a WS2812B strip through a small HTTP JSON endpoint."),
			TFlagArgument(&THttpServer::DEBUG, 'H', U"debug-http", U"", U"Enable HTTP server debug output"),
			TFlagArgument(&TFiber::DEBUG, 'F', U"debug-fiber", U"", U"Enable fiber debug output"),
			TPathArgument(&path_spi, EObjectType::CHAR_DEVICE, ECreateMode::OPEN, 'b', U"bus", U"", true, false, U"SPI device"),
			TPathArgument(&path_gpio_chip, EObjectType::CHAR_DEVICE, ECreateMode::OPEN, 'G', U"gpio-chip", U"", true, false, U"GPIO character device for optional level-shifter enable"),
			TIntegerArgument(&chip_enable_line, 'e', U"ce", U"", true, false, U"GPIO line controlling the level shifter; -1 disables it"),
			TIntegerArgument(&led_count, 'c', U"count", U"", true, false, U"Number of LEDs; -1 enables loopback auto-detection"),
			TIntegerArgument(&http_port, 'p', U"port", U"", true, false, U"HTTP listen port")
		);

		EL_ERROR(chip_enable_line < -1, TInvalidArgumentException, "ce", "-1 or a non-negative GPIO line");
		EL_ERROR(led_count == 0 || led_count < -1, TInvalidArgumentException, "count", "-1 or a positive LED count");
		EL_ERROR(http_port < 1 || http_port > 65535, TInvalidArgumentException, "port", "range 1-65535");

		std::unique_ptr<TNativeGpioController> gpio_controller;
		std::unique_ptr<IPin> chip_enable_pin;
		if(chip_enable_line >= 0)
		{
			gpio_controller = New<TNativeGpioController>(TFile(path_gpio_chip, TAccess::RW));
			chip_enable_pin = gpio_controller->ClaimPin(static_cast<usys_t>(chip_enable_line));
		}

		TNativeSpiBus spi_bus(path_spi);
		TLedStrip<TWS2812B> led_strip(
			spi_bus.ClaimDevice(std::move(chip_enable_pin)),
			static_cast<int>(led_count)
		);

		TTcpServer tcp_server(static_cast<port_t>(http_port));
		THttpServer http_server(&tcp_server, [&](const THttpServer::request_t& request, THttpServer::response_t& response)
		{
			response.status = EStatus::METHOD_NOT_ALLOWED;
			response.body = nullptr;

			if(request.method != EMethod::POST)
			{
				return;
			}

			EL_ERROR(request.body == nullptr, TInvalidArgumentException, "body", "POST request requires a JSON body");
			TStreamTextReader json_reader(request.body);
			auto json = TJsonValue::Parse(json_reader);
			const f64_t red = json["color"]["red"].ToDouble();
			const f64_t green = json["color"]["green"].ToDouble();
			const f64_t blue = json["color"]["blue"].ToDouble();
			EL_ERROR(red < 0 || red > 1, TInvalidArgumentException, "red", "range 0-1");
			EL_ERROR(green < 0 || green > 1, TInvalidArgumentException, "green", "range 0-1");
			EL_ERROR(blue < 0 || blue > 1, TInvalidArgumentException, "blue", "range 0-1");

			led_strip.UnifiedColor({
				static_cast<u8_t>(red * 255.0 + 0.5),
				static_cast<u8_t>(green * 255.0 + 0.5),
				static_cast<u8_t>(blue * 255.0 + 0.5)
			});
			led_strip.Apply();
			response.status = EStatus::OK;
		});

		std::fprintf(stderr, "Listening on port %lld. POST {\"color\":{\"red\":0..1,\"green\":0..1,\"blue\":0..1}}\n", static_cast<long long>(http_port));
		TFiber::Self()->Stop();
		return 0;
	}
	catch(const shutdown_t&)
	{
		return 0;
	}
	catch(const IException& exception)
	{
		exception.Print("TOP LEVEL");
		return 1;
	}
}
