#pragma once

#include "io_collection_list.hpp"
#include "io_stream.hpp"
#include "io_text_string.hpp"
#include "io_types.hpp"
#include "system_time.hpp"

namespace el1::dev::obd2::elm327
{
	using namespace io::types;

	enum class EProtocol : char32_t
	{
		AUTOMATIC = '0',
		SAE_J1850_PWM = '1',
		SAE_J1850_VPW = '2',
		ISO_9141_2 = '3',
		ISO_14230_4_KWP_5_BAUD = '4',
		ISO_14230_4_KWP_FAST = '5',
		ISO_15765_4_CAN_11_500 = '6',
		ISO_15765_4_CAN_29_500 = '7',
		ISO_15765_4_CAN_11_250 = '8',
		ISO_15765_4_CAN_29_250 = '9',
		SAE_J1939_CAN = 'A',
		USER_CAN_1 = 'B',
		USER_CAN_2 = 'C',
	};

	class TELM327
	{
		private:
			io::stream::IBinarySource* const source;
			io::stream::IBinarySink* const sink;
			const system::time::TTime command_timeout;

			void FlushInput();
			void ExpectOk(const io::text::string::TString& command);
			static void AppendHexLine(io::text::string::TString& hex, const io::text::string::TString& line);
			static u8_t DecodeHexDigit(const char32_t character);

		public:
			TELM327(io::stream::IBinarySource& source, io::stream::IBinarySink& sink, const system::time::TTime command_timeout = 5);

			io::text::string::TString Command(const io::text::string::TString& command);
			io::text::string::TString Reset();
			io::text::string::TString Initialize(const EProtocol protocol = EProtocol::AUTOMATIC);
			io::text::string::TString CloseProtocol();
			f64_t SupplyVoltage();
			void ConfigureIsoTpExtendedAddressing(const u16_t request_can_id, const u16_t response_can_id, const u8_t target_address);
			io::collection::list::TList<u8_t> ReadDataByIdentifier(const u16_t did);
			static io::collection::list::TList<u8_t> ParseReadDataByIdentifierResponse(const io::text::string::TString& response, const u16_t did);
	};
}
