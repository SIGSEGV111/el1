#include <el1/el1.cpp>

using namespace el1::io::types;
using namespace el1::io::file;
using namespace el1::io::net::ip;
using namespace el1::io::net::http;
using namespace el1::io::format::json;
using namespace el1::dev::spi::native;
using namespace el1::dev::spi::led;
using namespace el1::dev::spi::led::neopixel;
using namespace el1::dev::gpio;
using namespace el1::dev::gpio::native;
using namespace el1::system::cmdline;
using namespace el1::system::task;

int main(int argc, char* argv[])
{
	try
	{
		TPath path_spi_bus = "/dev/spidev0.0";
		s64_t idx_ce = -1;
		s64_t num_leds;
		s64_t http_port = 8080;

		ParseCmdlineArguments(argc, argv,
			TFlagArgument(&THttpServer::DEBUG, 'H', "debug-http", "", "Enable debug output for http-server"),
			TFlagArgument(&TFiber::DEBUG, 'F', "debug-fiber", "", "Enable debug output for fibers"),
			TPathArgument(&path_spi_bus, EObjectType::CHAR_DEVICE, ECreateMode::OPEN, 'b', "bus", "", true, false, "The SPI bus device to use"),
			TIntegerArgument(&idx_ce, 'e', "ce", "", true, false, "Configures the GPIO pin# that connects to the level-shifters for the LED strip"),
			TIntegerArgument(&num_leds, 'c', "count", "", false, false, "Count of LEDs on the strip. -1 to auto-detect."),
			TIntegerArgument(&http_port, 'p', "port", "", true, false, "HTTP-Server Port")
		);

		TNativeGpioController& gpio_ctrl = *TNativeGpioController::Instance();
		TNativeSpiBus spi_bus(path_spi_bus);
		TLedStrip<TWS2812B> led_strip(spi_bus.ClaimDevice((idx_ce < 0) ? std::unique_ptr<IPin>(nullptr) : gpio_ctrl.ClaimPin(idx_ce)), num_leds);

		TTcpServer tcp_server(http_port);
		THttpServer http_server(&tcp_server, [&](const THttpServer::request_t& req, THttpServer::response_t& res) {
			res.status = EStatus::METHOD_NOT_ALLOWED;
			res.body = nullptr;

			if(req.method == EMethod::POST)
			{
				EL_ERROR(req.body == nullptr, TException, "POST request requires a body");
				auto json = req.body->Pipe().Transform(TUTF8Decoder()).Transform(TJsonParser(false)).First();

				const u8_t r = json["color"]["red"  ].Number() * 255.0;
				const u8_t g = json["color"]["green"].Number() * 255.0;
				const u8_t b = json["color"]["blue" ].Number() * 255.0;

				led_strip.UnifiedColor({r,g,b});
				led_strip.Apply();

				res.status = EStatus::OK;
			}
		});

		fprintf(stderr, "READY!\n");
		TFiber::Self()->Stop();
		fprintf(stderr, "WTF?\n");
	}
	catch(shutdown_t)
	{
		fprintf(stderr, "\nBYE!\n");
	}
	catch(const IException& e)
	{
		e.Print("TOP LEVEL");
		return 1;
	}

	return 0;
}
