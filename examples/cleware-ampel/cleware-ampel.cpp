#include <el1/dev_usb_cleware_ampel.hpp>
#include <el1/error.hpp>
#include <el1/io_file.hpp>
#include <el1/io_text_string.hpp>
#include <el1/system_cmdline.hpp>

#include <cstdio>

int main(const int argc, char* argv[])
{
	using namespace el1;
	using namespace el1::dev::usb::cleware;
	using namespace el1::error;
	using namespace el1::io::file;
	using namespace el1::io::text::string;
	using namespace el1::system::cmdline;

	try
	{
		TPath device_path;
		TString signal = U"off";

		ParseCmdlineArguments(argc, argv,
			THelpArgument(U"Control a Cleware USB-Ampel (0d50:0008) via Linux hidraw."),
			TPathArgument(&device_path, EObjectType::CHAR_DEVICE, ECreateMode::OPEN, 'd', U"device", U"", true, false, U"Optional /dev/hidrawN device; auto-detected when omitted"),
			TStringArgument(&signal, 's', U"signal", U"", true, false, U"Signal: off, red, yellow or green")
		);

		TAmpel ampel = device_path.IsEmpty() ? TAmpel() : TAmpel(device_path);

		if(signal == U"off")
			ampel.SetSignal(ESignal::OFF);
		else if(signal == U"red")
			ampel.SetSignal(ESignal::RED);
		else if(signal == U"yellow")
			ampel.SetSignal(ESignal::YELLOW);
		else if(signal == U"green")
			ampel.SetSignal(ESignal::GREEN);
		else
			EL_THROW(TInvalidArgumentException, "signal", "off, red, yellow or green");

		std::printf("%s -> %s\n", static_cast<const char*>(ampel.device_path), signal.MakeCStr().get());
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
