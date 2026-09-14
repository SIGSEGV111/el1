#include "dev_gnss_nmea.hpp"

#include "io_text_parser.hpp"

#include <cmath>

namespace el1::dev::gnss::nmea
{
	using namespace error;
	using namespace io::text::parser;

	namespace
	{
		int hexValue(const char32_t chr)
		{
			if(chr >= U'0' && chr <= U'9')
				return static_cast<int>(chr - U'0');
			if(chr >= U'A' && chr <= U'F')
				return static_cast<int>(chr - U'A') + 10;
			if(chr >= U'a' && chr <= U'f')
				return static_cast<int>(chr - U'a') + 10;
			return -1;
		}

		auto makeUnsignedDecimalParser()
		{
			auto digit = CharRange(U'0', U'9');
			auto decimal = Capture(OneOrMore(digit) + Maybe(U'.'_P + OneOrMore(digit)));
			return Translate([](const TStringView value) { return value.ToDouble(); }, decimal);
		}

		auto makeDecimalParser()
		{
			auto digit = CharRange(U'0', U'9');
			auto decimal = Capture(
				Maybe(CharList(U'+', U'-')) +
				OneOrMore(digit) +
				Maybe(U'.'_P + OneOrMore(digit))
			);
			return Translate([](const TStringView value) { return value.ToDouble(); }, decimal);
		}

		auto makeUnsignedParser()
		{
			return Translate(
				[](const TStringView value) { return static_cast<u64_t>(value.ToInteger()); },
				Capture(OneOrMore(CharRange(U'0', U'9')))
			);
		}

		std::optional<double> parseUnsignedDecimal(const TStringView value)
		{
			if(value.Length() == 0)
				return std::nullopt;
			try
			{
				return makeUnsignedDecimalParser().Parse(value);
			}
			catch(const IException&)
			{
				return std::nullopt;
			}
		}

		std::optional<double> parseDecimal(const TStringView value)
		{
			if(value.Length() == 0)
				return std::nullopt;
			try
			{
				return makeDecimalParser().Parse(value);
			}
			catch(const IException&)
			{
				return std::nullopt;
			}
		}

		std::optional<u64_t> parseUnsigned(const TStringView value)
		{
			if(value.Length() == 0)
				return std::nullopt;
			try
			{
				return makeUnsignedParser().Parse(value);
			}
			catch(const IException&)
			{
				return std::nullopt;
			}
		}

		bool validateChecksum(const TStringView payload, const TStringView checksum_text)
		{
			if(checksum_text.Length() != 2)
				return false;

			u8_t checksum = 0;
			for(const char32_t chr : payload)
			{
				if(chr > 0xff)
					return false;
				checksum ^= static_cast<u8_t>(chr);
			}

			const int high = hexValue(checksum_text[0]);
			const int low = hexValue(checksum_text[1]);
			return high >= 0 && low >= 0 && checksum == static_cast<u8_t>((high << 4) | low);
		}

		auto makeEnvelopeParser()
		{
			auto payload = Discard(U'$'_P) + Capture(OneOrMore(~CharList(U'*', U'\r', U'\n')));
			auto hex = CharRange(U'0', U'9') || CharRange(U'A', U'F') || CharRange(U'a', U'f');
			auto checksum = Maybe(Discard(U'*'_P) + Capture(Repeat(2, 2, hex)));
			auto suffix = checksum + Discard(Maybe(U'\r'_P)) + Discard(Maybe(U'\n'_P)) + End();

			return TryTranslate(
				[](const TStringView payload_text, const std::optional<TStringView> checksum_text) -> std::optional<TString>
				{
					if(checksum_text.has_value() && !validateChecksum(payload_text, *checksum_text))
						return std::nullopt;
					return TString(payload_text);
				},
				payload,
				suffix
			);
		}

		auto makeFieldParser()
		{
			return Capture(Repeat(0, NEG1, ~CharList(U',', U'*', U'\r', U'\n')));
		}

		auto makeTailParser()
		{
			auto field = makeFieldParser();
			return Discard(Repeat(0, NEG1, Discard(U','_P) + field)) + End();
		}
	}

	std::optional<double> TParser::parseCoordinate(const TStringView value, const TStringView hemisphere)
	{
		const std::optional<double> raw_value = parseUnsignedDecimal(value);
		if(!raw_value.has_value())
			return std::nullopt;

		if(hemisphere != U"N" && hemisphere != U"S" && hemisphere != U"E" && hemisphere != U"W")
			return std::nullopt;

		const double degrees = std::floor(*raw_value / 100.0);
		const double minutes = *raw_value - degrees * 100.0;
		if(minutes < 0.0 || minutes >= 60.0)
			return std::nullopt;
		if((hemisphere == U"N" || hemisphere == U"S") && degrees > 90.0)
			return std::nullopt;
		if((hemisphere == U"E" || hemisphere == U"W") && degrees > 180.0)
			return std::nullopt;

		double coordinate = degrees + minutes / 60.0;
		if(hemisphere == U"S" || hemisphere == U"W")
			coordinate = -coordinate;
		return coordinate;
	}

	std::optional<TFix> TParser::parseSentence(const TStringView sentence)
	{
		TString payload;
		try
		{
			payload = makeEnvelopeParser().Parse(sentence);
		}
		catch(const IException&)
		{
			return std::nullopt;
		}

		auto field = makeFieldParser();
		auto talker = Repeat(2, 2, CharRange(U'A', U'Z'));
		auto tail = makeTailParser();

		auto gga = TryTranslate(
			[this](
				const TStringView latitude,
				const TStringView north_south,
				const TStringView longitude,
				const TStringView east_west,
				const TStringView quality_text,
				const TStringView satellites_text,
				const TStringView hdop_text,
				const TStringView altitude_text
			) -> std::optional<TFix>
			{
				const std::optional<u64_t> quality = parseUnsigned(quality_text);
				if(!quality.has_value())
					return std::nullopt;

				TFix result = fix;
				result.valid = *quality > 0;
				if(!result.valid)
					return result;

				const std::optional<double> latitude_deg = parseCoordinate(latitude, north_south);
				const std::optional<double> longitude_deg = parseCoordinate(longitude, east_west);
				if(!latitude_deg.has_value() || !longitude_deg.has_value())
					return std::nullopt;
				result.latitude_deg = *latitude_deg;
				result.longitude_deg = *longitude_deg;

				if(satellites_text.Length() > 0)
				{
					const std::optional<u64_t> satellites = parseUnsigned(satellites_text);
					if(!satellites.has_value())
						return std::nullopt;
					result.satellites = static_cast<u32_t>(*satellites);
				}

				if(hdop_text.Length() > 0)
				{
					const std::optional<double> hdop = parseUnsignedDecimal(hdop_text);
					if(!hdop.has_value())
						return std::nullopt;
					result.horizontal_dop = *hdop;
					result.has_horizontal_dop = true;
				}

				if(altitude_text.Length() > 0)
				{
					const std::optional<double> altitude = parseDecimal(altitude_text);
					if(!altitude.has_value())
						return std::nullopt;
					result.altitude_m = *altitude;
					result.has_altitude = true;
				}

				return result;
			},
			Discard(talker + U"GGA,"_P + field + Discard(U','_P)) + field,
			Discard(U','_P) + field,
			Discard(U','_P) + field,
			Discard(U','_P) + field,
			Discard(U','_P) + field,
			Discard(U','_P) + field,
			Discard(U','_P) + field,
			Discard(U','_P) + field + tail
		);

		auto rmc = TryTranslate(
			[this](
				const TStringView status,
				const TStringView latitude,
				const TStringView north_south,
				const TStringView longitude,
				const TStringView east_west,
				const TStringView speed_text,
				const TStringView heading_text
			) -> std::optional<TFix>
			{
				if(status != U"A" && status != U"V")
					return std::nullopt;

				TFix result = fix;
				result.valid = status == U"A";
				if(!result.valid)
					return result;

				const std::optional<double> latitude_deg = parseCoordinate(latitude, north_south);
				const std::optional<double> longitude_deg = parseCoordinate(longitude, east_west);
				if(!latitude_deg.has_value() || !longitude_deg.has_value())
					return std::nullopt;
				result.latitude_deg = *latitude_deg;
				result.longitude_deg = *longitude_deg;

				if(speed_text.Length() > 0)
				{
					const std::optional<double> speed_knots = parseUnsignedDecimal(speed_text);
					if(!speed_knots.has_value())
						return std::nullopt;
					static constexpr double KNOTS_TO_MPS = 0.5144444444444444;
					result.speed_mps = *speed_knots * KNOTS_TO_MPS;
					result.has_speed = true;
				}

				if(heading_text.Length() > 0)
				{
					const std::optional<double> heading = parseUnsignedDecimal(heading_text);
					if(!heading.has_value())
						return std::nullopt;
					result.heading_deg = *heading;
					result.has_heading = true;
				}

				return result;
			},
			Discard(talker + U"RMC,"_P + field + Discard(U','_P)) + field,
			Discard(U','_P) + field,
			Discard(U','_P) + field,
			Discard(U','_P) + field,
			Discard(U','_P) + field,
			Discard(U','_P) + field,
			Discard(U','_P) + field + tail
		);

		try
		{
			fix = (gga || rmc).Parse(payload);
			return fix;
		}
		catch(const IException&)
		{
			return std::nullopt;
		}
	}

	std::optional<TFix> TParser::feedByte(const byte_t byte)
	{
		if(byte == '\r')
			return std::nullopt;

		if(byte == '\n')
		{
			if(line.Length() == 0)
				return std::nullopt;
			TString sentence = std::move(line);
			line = TString();
			return parseSentence(sentence);
		}

		if(byte < 0x20 || byte > 0x7e)
			return std::nullopt;

		if(line.Length() >= MAX_SENTENCE_LENGTH)
		{
			line = TString();
			return std::nullopt;
		}

		if(byte == '$')
			line = TString();

		line += static_cast<char32_t>(byte);
		return std::nullopt;
	}

	std::optional<TFix> TParser::feedSentence(const TStringView sentence)
	{
		return parseSentence(sentence);
	}
}
