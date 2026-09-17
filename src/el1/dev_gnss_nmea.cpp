#include "dev_gnss_nmea.hpp"

#include "io_text_parser.hpp"

#include <cmath>
#include <limits>

namespace el1::dev::gnss::nmea
{
	using namespace error;
	using namespace io::text::parser;

	namespace
	{
		using TFields = io::collection::list::TList<TStringView>;

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

		bool validateChecksum(const TStringView payload, const TStringView checksum_text)
		{
			if(checksum_text.Length() != 2)
				return false;

			u8_t checksum = 0;
			for(const char32_t chr : payload)
				checksum ^= static_cast<u8_t>(chr);

			const int high = hexValue(checksum_text[0]);
			const int low = hexValue(checksum_text[1]);
			return high >= 0 && low >= 0 && checksum == static_cast<u8_t>((high << 4) | low);
		}

		auto makeEnvelopeGrammar()
		{
			auto payload_char = Where([](const char32_t chr) { return chr != U'*'; }, CharRange(U' ', U'~'));
			auto payload = Capture(OneOrMore(payload_char));
			auto hex = CharRange(U'0', U'9') || CharRange(U'A', U'F') || CharRange(U'a', U'f');
			auto checksum = Maybe(Discard(U'*'_P) + Capture(Repeat(2, 2, hex)));
			auto end = Discard(Maybe(U'\r'_P)) + Discard(Maybe(U'\n'_P)) + End();

			return TryTranslate(
				[](const TStringView payload, const std::optional<TStringView> checksum) -> std::optional<TStringView>
				{
					return !checksum.has_value() || validateChecksum(payload, *checksum) ? std::optional<TStringView>(payload) : std::nullopt;
				},
				Discard(U'$'_P) + payload,
				checksum + end
			);
		}

		auto makePayloadGrammar()
		{
			auto talker = Repeat(2, 2, CharRange(U'A', U'Z'));
			auto field = Discard(U','_P) + Capture(Repeat(0, NEG1, ~CharList(U',', U'*', U'\r', U'\n')));
			auto gga = Capture(talker + U"GGA"_P) + Repeat(9, NEG1, field) + End();
			auto rmc = Capture(talker + U"RMC"_P) + Repeat(8, NEG1, field) + End();

			return gga || rmc;
		}

		std::optional<double> parseDecimal(const TStringView text, const bool allow_negative = false)
		{
			static const auto DIGIT = CharRange(U'0', U'9');
			static const auto UNSIGNED = Capture(OneOrMore(DIGIT) + Maybe(U'.'_P + OneOrMore(DIGIT))) + End();
			static const auto SIGNED = Capture(Maybe(U'-'_P) + OneOrMore(DIGIT) + Maybe(U'.'_P + OneOrMore(DIGIT))) + End();
			try
			{
				return (allow_negative ? SIGNED.Parse(text) : UNSIGNED.Parse(text)).ToDouble();
			}
			catch(const IException&)
			{
				return std::nullopt;
			}
		}

		std::optional<u32_t> parseUnsigned(const TStringView text)
		{
			static const auto UNSIGNED = Capture(Repeat(1, 10, CharRange(U'0', U'9'))) + End();
			try
			{
				const u64_t value = static_cast<u64_t>(UNSIGNED.Parse(text).ToInteger());
				return value <= std::numeric_limits<u32_t>::max() ? std::optional<u32_t>(static_cast<u32_t>(value)) : std::nullopt;
			}
			catch(const IException&)
			{
				return std::nullopt;
			}
		}

		std::optional<double> parseCoordinate(const TStringView value, const TStringView hemisphere, const bool latitude)
		{
			const std::optional<double> raw = parseDecimal(value);
			if(!raw.has_value() || hemisphere.Length() != 1)
				return std::nullopt;

			const char32_t positive = latitude ? U'N' : U'E';
			const char32_t negative = latitude ? U'S' : U'W';
			if(hemisphere[0] != positive && hemisphere[0] != negative)
				return std::nullopt;

			const double degrees = std::floor(*raw / 100.0);
			const double minutes = *raw - degrees * 100.0;
			const double limit = latitude ? 90.0 : 180.0;
			if(minutes >= 60.0 || degrees > limit || (degrees == limit && minutes != 0.0))
				return std::nullopt;

			const double coordinate = degrees + minutes / 60.0;
			return hemisphere[0] == negative ? -coordinate : coordinate;
		}

		template<typename T, typename F>
		bool updateOptional(const TStringView text, std::optional<T>& target, F parse)
		{
			if(text.Length() == 0)
				return true;
			const std::optional<T> value = parse(text);
			if(!value.has_value())
				return false;
			target = *value;
			return true;
		}

		std::optional<TFix> parseGga(const TFix& current, const TFields& field)
		{
			const TStringView latitude_text = field[2];
			const TStringView north_south = field[3];
			const TStringView longitude_text = field[4];
			const TStringView east_west = field[5];

			const std::optional<u32_t> quality = parseUnsigned(field[6]);
			if(!quality.has_value())
				return std::nullopt;

			TFix result = current;
			result.valid = *quality > 0;
			if(!result.valid)
				return result;

			const auto latitude = parseCoordinate(latitude_text, north_south, true);
			const auto longitude = parseCoordinate(longitude_text, east_west, false);
			if(!latitude.has_value() || !longitude.has_value())
				return std::nullopt;
			result.latitude_deg = *latitude;
			result.longitude_deg = *longitude;

			if(!updateOptional(field[7], result.satellites, parseUnsigned) ||
				!updateOptional(field[8], result.horizontal_dop, [](const TStringView value) { return parseDecimal(value); }) ||
				!updateOptional(field[9], result.altitude_m, [](const TStringView value) { return parseDecimal(value, true); }))
				return std::nullopt;

			return result;
		}

		std::optional<TFix> parseRmc(const TFix& current, const TFields& field)
		{
			const TStringView status = field[2];
			const TStringView latitude_text = field[3];
			const TStringView north_south = field[4];
			const TStringView longitude_text = field[5];
			const TStringView east_west = field[6];
			if(status != U"A" && status != U"V")
				return std::nullopt;

			TFix result = current;
			result.valid = status == U"A";
			if(!result.valid)
				return result;

			const auto latitude = parseCoordinate(latitude_text, north_south, true);
			const auto longitude = parseCoordinate(longitude_text, east_west, false);
			if(!latitude.has_value() || !longitude.has_value())
				return std::nullopt;
			result.latitude_deg = *latitude;
			result.longitude_deg = *longitude;

			static constexpr double KNOTS_TO_MPS = 0.5144444444444444;
			if(!updateOptional(field[7], result.speed_mps, [](const TStringView value) -> std::optional<double>
			{
				const auto knots = parseDecimal(value);
				return knots.has_value() ? std::optional<double>(*knots * KNOTS_TO_MPS) : std::nullopt;
			}) || !updateOptional(field[8], result.heading_deg, [](const TStringView value) { return parseDecimal(value); }))
				return std::nullopt;

			return result;
		}
	}

	std::optional<TFix> TParser::parseSentence(const TStringView sentence)
	{
		static const auto ENVELOPE = makeEnvelopeGrammar();
		static const auto PAYLOAD = makePayloadGrammar();

		try
		{
			const TStringView payload = ENVELOPE.Parse(sentence);
			const TFields fields = PAYLOAD.Parse(payload);

			std::optional<TFix> result;
			if(fields[0].SliceSL(2) == U"GGA")
				result = parseGga(fix, fields);
			else if(fields[0].SliceSL(2) == U"RMC")
				result = parseRmc(fix, fields);

			if(result.has_value())
				fix = *result;
			return result;
		}
		catch(const IException&)
		{
			return std::nullopt;
		}
	}
}
