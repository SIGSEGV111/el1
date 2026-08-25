#include "def.hpp"
#ifdef EL_OS_LINUX

#include "dev_usb_cleware_ampel.hpp"
#include "error.hpp"
#include "io_text_string.hpp"

#include <linux/hidraw.h>
#include <linux/input.h>
#include <sys/ioctl.h>

namespace el1::dev::usb::cleware
{
	using namespace error;
	using namespace io::text::string;

	static bool IsClewareAmpel(const TString& uevent)
	{
		return uevent.Contains(U":00000D50:00000008") || uevent.Contains(U":00000d50:00000008");
	}

	report_t TAmpel::BuildReport(const EColor color, const ELightState state)
	{
		return {
			.report_id = 0x00,
			.payload_prefix = 0x00,
			.color = static_cast<byte_t>(color),
			.state = static_cast<byte_t>(state),
		};
	}

	TPath TAmpel::FindDevicePath()
	{
		const TPath class_path("/sys/class/hidraw");
		TDirectory class_dir(class_path);
		TPath result;
		usys_t n_matches = 0;

		class_dir.Enum(
			[&](direntry_t& entry)
			{
				if(!entry.name.BeginsWith(U"hidraw"))
					return true;

				const TPath uevent_path = class_path + TPath(entry.name) + TPath("device/uevent");
				if(!uevent_path.Exists())
					return true;

				const TString uevent = TFile::ReadText(uevent_path, false, 4096);
				if(!IsClewareAmpel(uevent))
					return true;

				result = TPath("/dev") + TPath(entry.name);
				n_matches++;
				return true;
			}
		);

		EL_ERROR(n_matches == 0, TException, TString(U"Cleware USB-Ampel 0d50:0008 not found via /sys/class/hidraw"));
		EL_ERROR(n_matches > 1, TException, TString(U"multiple Cleware USB-Ampel devices found; construct TAmpel with an explicit /dev/hidrawN path"));
		return result;
	}

	void TAmpel::ValidateDevice()
	{
		hidraw_devinfo info = {};
		EL_SYSERR(ioctl(device.Handle(), HIDIOCGRAWINFO, &info));
		EL_ERROR(info.bustype != BUS_USB || info.vendor != VENDOR_ID || info.product != PRODUCT_ID, TInvalidArgumentException, "device_path", "device is not a Cleware USB-Ampel 0d50:0008");
	}

	void TAmpel::WriteReport(const report_t& report)
	{
		device.WriteAll(reinterpret_cast<const byte_t*>(&report), sizeof(report));
	}

	void TAmpel::SetLight(const EColor color, const ELightState state)
	{
		WriteReport(BuildReport(color, state));
	}

	void TAmpel::SetState(const bool red, const bool yellow, const bool green)
	{
		SetLight(EColor::RED, red ? ELightState::ON : ELightState::OFF);
		SetLight(EColor::YELLOW, yellow ? ELightState::ON : ELightState::OFF);
		SetLight(EColor::GREEN, green ? ELightState::ON : ELightState::OFF);
	}

	void TAmpel::SetSignal(const ESignal signal)
	{
		switch(signal)
		{
			case ESignal::OFF:
				SetState(false, false, false);
				break;

			case ESignal::RED:
				SetState(true, false, false);
				break;

			case ESignal::YELLOW:
				SetState(false, true, false);
				break;

			case ESignal::GREEN:
				SetState(false, false, true);
				break;
		}
	}

	void TAmpel::TurnOff()
	{
		SetSignal(ESignal::OFF);
	}

	TAmpel::TAmpel(TPath path) : device(path, TAccess::RW), device_path(path)
	{
		ValidateDevice();
	}

	TAmpel::TAmpel() : TAmpel(FindDevicePath())
	{
	}
}

#endif
