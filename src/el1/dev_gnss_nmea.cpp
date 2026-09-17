#include "dev_gnss_nmea.hpp"

#include "io_text_parser.hpp"
#include "io_text_number.hpp"

#include <cmath>

namespace el1::dev::gnss::nmea
{
	using namespace error;
	using namespace io::text::parser;

	namespace
	{
		using TFields = io::collection::list::TList<TStringView>;

		bool validateChecksum(const TStringView payload, const u8_t expected)
		{
			u8_t checksum = 0;
			for(const char32_t chr : payload)
				checksum ^= static_cast<u8_t>(chr);
			return checksum == expected;
		}

		auto makeEnvelopeGrammar()
		{
			auto payload_char = Where([](const char32_t chr) { return chr != U'*'; }, CharRange(U' ', U'~'));
			auto payload = Capture(OneOrMore(payload_char));
			auto hex_digit = CharRange(U'0', U'9') || CharRange(U'A', U'F') || CharRange(U'a', U'f');
			auto checksum = Maybe(Discard(U'*'_P) + TryTranslate(
				[](const TStringView text) { return io::text::number::TryParseHex<u8_t>(text); },
				Capture(Repeat(2, 2, hex_digit))
			));
			auto end = Discard(Maybe(U'\r'_P)) + Discard(Maybe(U'\n'_P)) + End();

			return TryTranslate(
				[](const TStringView payload, const std::optional<u8_t> checksum) -> std::optional<TStringView>
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
			static const auto DECIMAL = TryTranslate(
				[](const TStringView value) { return io::text::number::TryParseDecimal<double>(value); },
				Capture(Maybe(U'-'_P) + OneOrMore(DIGIT) + Maybe(U'.'_P + OneOrMore(DIGIT))) + End()
			);

			if(!allow_negative && text.BeginsWith(U"-"))
				return std::nullopt;
			try { return DECIMAL.Parse(text); }
			catch(const IException&) { return std::nullopt; }
		}

		std::optional<u32_t> parseUnsigned(const TStringView text)
		{
			return io::text::number::TryParseInteger<u32_t>(text);
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
		bool parseOptional(const TStringView text, std::optional<T>& value, F parse)
		{
			if(text.Length() == 0)
				return true;
			value = parse(text);
			return value.has_value();
		}

		bool parseGga(TFix& fix, const TFields& field)
		{
			const std::optional<u32_t> quality = parseUnsigned(field[6]);
			if(!quality.has_value())
				return false;

			if(*quality == 0)
			{
				fix.valid = false;
				return true;
			}

			const std::optional<double> latitude = parseCoordinate(field[2], field[3], true);
			const std::optional<double> longitude = parseCoordinate(field[4], field[5], false);
			std::optional<u32_t> satellites;
			std::optional<double> horizontal_dop;
			std::optional<double> altitude_m;

			if(!latitude.has_value() || !longitude.has_value() ||
				!parseOptional(field[7], satellites, parseUnsigned) ||
				!parseOptional(field[8], horizontal_dop, [](const TStringView value) { return parseDecimal(value); }) ||
				!parseOptional(field[9], altitude_m, [](const TStringView value) { return parseDecimal(value, true); }))
				return false;

			fix.valid = true;
			fix.latitude_deg = *latitude;
			fix.longitude_deg = *longitude;
			if(satellites.has_value())
				fix.satellites = satellites;
			if(horizontal_dop.has_value())
				fix.horizontal_dop = horizontal_dop;
			if(altitude_m.has_value())
				fix.altitude_m = altitude_m;
			return true;
		}

		bool parseRmc(TFix& fix, const TFields& field)
		{
			const TStringView status = field[2];
			if(status != U"A" && status != U"V")
				return false;

			if(status == U"V")
			{
				fix.valid = false;
				return true;
			}

			const std::optional<double> latitude = parseCoordinate(field[3], field[4], true);
			const std::optional<double> longitude = parseCoordinate(field[5], field[6], false);
			std::optional<double> speed_mps;
			std::optional<double> heading_deg;

			static constexpr double KNOTS_TO_MPS = 0.5144444444444444;
			if(!latitude.has_value() || !longitude.has_value() ||
				!parseOptional(field[7], speed_mps, [](const TStringView value) -> std::optional<double>
				{
					const auto knots = parseDecimal(value);
					return knots.has_value() ? std::optional<double>(*knots * KNOTS_TO_MPS) : std::nullopt;
				}) ||
				!parseOptional(field[8], heading_deg, [](const TStringView value) { return parseDecimal(value); }))
				return false;

			fix.valid = true;
			fix.latitude_deg = *latitude;
			fix.longitude_deg = *longitude;
			if(speed_mps.has_value())
				fix.speed_mps = speed_mps;
			if(heading_deg.has_value())
				fix.heading_deg = heading_deg;
			return true;
		}

	}

	TFix* TParser::parseSentence(const TStringView sentence)
	{
		static const auto ENVELOPE = makeEnvelopeGrammar();
		static const auto PAYLOAD = makePayloadGrammar();

		try
		{
			const TStringView payload = ENVELOPE.Parse(sentence);
			const TFields fields = PAYLOAD.Parse(payload);

			const TStringView type = fields[0].SliceSL(2);
			const bool parsed = type == U"GGA" ? parseGga(fix, fields) : parseRmc(fix, fields);
			return parsed ? &fix : nullptr;
		}
		catch(const IException&)
		{
			return nullptr;
		}
	}
}
