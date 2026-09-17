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
		std::optional<double> altitude_m;
		std::optional<double> speed_mps;
		std::optional<double> heading_deg;
		std::optional<double> horizontal_dop;
		std::optional<u32_t> satellites;
	};

	class TParser
	{
		private:
			TFix fix;

		public:
			using TIn = TString;
			using TOut = TFix;

			static constexpr usys_t MAX_SENTENCE_LENGTH = 1024;

			TFix* parseSentence(TStringView sentence);

			template<typename TSourceStream>
			TFix* NextItem(TSourceStream* const source)
			{
				const TString* sentence;
				while((sentence = source->NextItem()) != nullptr)
					if(TFix* const parsed = parseSentence(*sentence))
						return parsed;
				return nullptr;
			}
	};
}
