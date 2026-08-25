#pragma once

#include "def.hpp"
#include "io_file.hpp"
#include "io_types.hpp"

namespace el1::dev::usb::cleware
{
	using namespace io::file;
	using namespace io::types;

	enum class EColor : u8_t
	{
		RED = 0x10,
		YELLOW = 0x11,
		GREEN = 0x12,
	};

	enum class ELightState : u8_t
	{
		OFF = 0x00,
		ON = 0x01,
		BLINK_500_MS = 0x10,
		BLINK_1_S = 0x11,
		BLINK_2_S = 0x12,
	};

	enum class ESignal : u8_t
	{
		OFF,
		RED,
		YELLOW,
		GREEN,
	};

	struct report_t
	{
		byte_t report_id;
		byte_t payload_prefix;
		byte_t color;
		byte_t state;
	};

	static_assert(sizeof(report_t) == 4);

	class TAmpel
	{
		private:
			TFile device;

			void ValidateDevice();
			void WriteReport(const report_t& report);

		public:
			static constexpr u16_t VENDOR_ID = 0x0d50;
			static constexpr u16_t PRODUCT_ID = 0x0008;

			const TPath device_path;

			static report_t BuildReport(const EColor color, const ELightState state);
			static TPath FindDevicePath();

			void SetLight(const EColor color, const ELightState state);
			void SetState(const bool red, const bool yellow, const bool green);
			void SetSignal(const ESignal signal);
			void TurnOff();

			explicit TAmpel(TPath device_path);
			TAmpel();
	};
}
