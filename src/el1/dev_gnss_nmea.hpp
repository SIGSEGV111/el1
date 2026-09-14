#pragma once

#include "def.hpp"
#include "io_text_string.hpp"
#include "io_types.hpp"

#include <optional>

namespace el1::dev::gnss::nmea
{
	using namespace io::types;
	using namespace io::text::string;

	struct TFix
	{
		bool valid = false;
		double latitude_deg = 0.0;
		double longitude_deg = 0.0;
		double altitude_m = 0.0;
		double speed_mps = -1.0;
		double heading_deg = -1.0;
		double horizontal_dop = 0.0;
		u32_t satellites = 0;
		bool has_altitude = false;
		bool has_speed = false;
		bool has_heading = false;
		bool has_horizontal_dop = false;
	};

	class TParser
	{
		private:
			TString line;
			TFix fix;

			static std::optional<double> parseCoordinate(TStringView value, TStringView hemisphere);
			std::optional<TFix> parseSentence(TStringView sentence);

		public:
			static constexpr usys_t MAX_SENTENCE_LENGTH = 1024;

			std::optional<TFix> feedByte(byte_t byte);
			std::optional<TFix> feedSentence(TStringView sentence);
			const TFix& currentFix() const EL_GETTER { return fix; }
	};
}
