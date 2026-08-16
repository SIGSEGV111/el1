#include "dev_obd2_elm327.hpp"
#include "error.hpp"

namespace el1::dev::obd2::elm327
{
	using namespace error;
	using namespace io::collection::list;
	using namespace io::stream;
	using namespace io::text::string;
	using namespace system::time;

	namespace
	{
		bool IsAsciiWhitespace(const char32_t character)
		{
			return character == ' ' || character == '\t' || character == '\r' || character == '\n' || character == '\f' || character == '\v';
		}

		bool IsHexDigit(const char32_t character)
		{
			return (character >= '0' && character <= '9') || (character >= 'A' && character <= 'F');
		}
	}

	TELM327::TELM327(IBinarySource& source, IBinarySink& sink, const TTime command_timeout) :
		source(&source),
		sink(&sink),
		command_timeout(command_timeout)
	{
		EL_ERROR(command_timeout <= 0, TInvalidArgumentException, "command_timeout", "must be greater than zero");
	}

	void TELM327::FlushInput()
	{
		byte_t buffer[512];
		for(;;)
		{
			const usys_t size = source->Read(buffer, sizeof(buffer));
			if(size > 0)
				continue;
			EL_ERROR(source->OnInputReady() == nullptr, TStreamDryException);
			return;
		}
	}

	TString TELM327::Command(const TString& command)
	{
		FlushInput();

		TString request = command;
		request += '\r';
		auto request_cstr = request.MakeCStr();
		const TTime deadline = TTime::Now(EClock::MONOTONIC) + command_timeout;
		const usys_t n_written = sink->BlockingWrite(
			reinterpret_cast<const byte_t*>(request_cstr.get()),
			request.Length(),
			deadline,
			true
		);
		EL_ERROR(n_written != request.Length(), TException, U"ELM327 command write timed out");

		TList<char> response(1024);
		for(;;)
		{
			byte_t buffer[512];
			const usys_t size = source->Read(buffer, sizeof(buffer));
			if(size > 0)
			{
				EL_ERROR(response.Count() + size > 65536, TException, U"ELM327 response exceeded 64 KiB");
				response.Append(reinterpret_cast<const char*>(buffer), size);
				for(usys_t i = 0; i < size; i++)
					if(buffer[i] == static_cast<byte_t>('>'))
						return TString(response.ItemPtr(0), response.Count());
				continue;
			}

			const auto* const input_ready = source->OnInputReady();
			EL_ERROR(input_ready == nullptr, TStreamDryException);
			EL_ERROR(!input_ready->WaitFor(deadline, true), TException, TString::Format(U"ELM327 command %q timed out", command));
		}
	}

	TString TELM327::Reset()
	{
		return Command(U"ATZ");
	}

	void TELM327::ExpectOk(const TString& command)
	{
		const TString response = Command(command);
		EL_ERROR(!response.Upper().Contains(TStringView(U"OK")), TException, TString::Format(U"ELM327 adapter rejected %q: %q", command, response));
	}

	TString TELM327::Initialize(const EProtocol protocol)
	{
		const TString reset_response = Reset();
		const TList<TString> commands = {
			U"ATE0",
			U"ATL0",
			U"ATS0",
			U"ATH0",
			U"ATAL",
			U"ATAT1",
			U"ATCAF1",
			U"ATCFC1"
		};
		for(const TString& command : commands)
			ExpectOk(command);

		const char32_t protocol_code = static_cast<char32_t>(protocol);
		EL_ERROR(!((protocol_code >= '0' && protocol_code <= '9') || (protocol_code >= 'A' && protocol_code <= 'C')), TInvalidArgumentException, "protocol", "invalid ELM327 protocol code");
		TString protocol_command(U"ATSP");
		protocol_command += protocol_code;
		ExpectOk(protocol_command);
		return reset_response;
	}

	TString TELM327::CloseProtocol()
	{
		return Command(U"ATPC");
	}

	f64_t TELM327::SupplyVoltage()
	{
		const TString response = Command(U"ATRV").Upper();
		const usys_t suffix = response.Find('V');
		EL_ERROR(suffix == NEG1, TException, TString::Format(U"invalid ELM327 voltage response: %q", response));

		usys_t begin = suffix;
		while(begin > 0)
		{
			const char32_t character = response[begin - 1];
			if((character < '0' || character > '9') && character != '.' && character != '-')
				break;
			begin--;
		}
		EL_ERROR(begin == suffix, TException, TString::Format(U"invalid ELM327 voltage response: %q", response));

		return response.View().SliceBE(begin, suffix).ToDouble();
	}

	bool TELM327::WaitForBusActivity(const TTime timeout)
	{
		EL_ERROR(timeout <= 0, TInvalidArgumentException, "timeout", "must be greater than zero");

		// Reset any receive-address filter left by a previous physical ECU request.
		// ATMA itself is passive on CAN: it does not acknowledge or transmit bus frames.
		ExpectOk(U"ATAR");
		FlushInput();

		static constexpr char MONITOR_COMMAND[] = "ATMA\r";
		const TTime monitor_deadline = TTime::Now(EClock::MONOTONIC) + timeout;
		const usys_t n_written = sink->BlockingWrite(
			reinterpret_cast<const byte_t*>(MONITOR_COMMAND),
			sizeof(MONITOR_COMMAND) - 1,
			monitor_deadline,
			true
		);
		EL_ERROR(n_written != sizeof(MONITOR_COMMAND) - 1, TException, U"ELM327 monitor command write timed out");

		bool activity = false;
		bool prompt_seen = false;
		while(!activity && !prompt_seen)
		{
			byte_t buffer[512];
			const usys_t size = source->Read(buffer, sizeof(buffer));
			for(usys_t i = 0; i < size; i++)
			{
				const char character = static_cast<char>(buffer[i]);
				if(character == '>')
					prompt_seen = true;
				else if(character != ' ' && character != '\t' && character != '\r' && character != '\n')
					activity = true;
			}

			if(activity || prompt_seen)
				break;
			const auto* const input_ready = source->OnInputReady();
			EL_ERROR(input_ready == nullptr, TStreamDryException);
			if(!input_ready->WaitFor(monitor_deadline, true))
				break;
		}

		EL_ERROR(prompt_seen, TException, U"ELM327 left monitor mode unexpectedly");

		// Any serial input stops ATMA. CR is harmless and gives us a prompt to
		// synchronize the byte stream before issuing the next command.
		static constexpr byte_t STOP_MONITOR = '\r';
		const TTime stop_deadline = TTime::Now(EClock::MONOTONIC) + command_timeout;
		EL_ERROR(sink->BlockingWrite(&STOP_MONITOR, 1, stop_deadline, true) != 1, TException, U"ELM327 monitor stop write timed out");

		for(;;)
		{
			byte_t buffer[512];
			const usys_t size = source->Read(buffer, sizeof(buffer));
			for(usys_t i = 0; i < size; i++)
				if(buffer[i] == static_cast<byte_t>('>'))
					return activity;

			const auto* const input_ready = source->OnInputReady();
			EL_ERROR(input_ready == nullptr, TStreamDryException);
			EL_ERROR(!input_ready->WaitFor(stop_deadline, true), TException, U"ELM327 did not leave monitor mode");
		}
	}

	void TELM327::ConfigureIsoTpExtendedAddressing(const u16_t request_can_id, const u16_t response_can_id, const u8_t target_address)
	{
		EL_ERROR(request_can_id > 0x7FF, TInvalidArgumentException, "request_can_id", "must be an 11-bit CAN identifier");
		EL_ERROR(response_can_id > 0x7FF, TInvalidArgumentException, "response_can_id", "must be an 11-bit CAN identifier");

		const TList<TString> commands = {
			TString::Format(U"ATSH%03x", request_can_id).Upper(),
			TString::Format(U"ATCRA%03x", response_can_id).Upper(),
			TString::Format(U"ATFCSH%03x", request_can_id).Upper(),
			TString::Format(U"ATFCSD%02x30FF00", target_address).Upper(),
			U"ATFCSM1",
			TString::Format(U"ATCEA%02x", target_address).Upper()
		};
		for(const TString& command : commands)
			ExpectOk(command);
	}

	u8_t TELM327::DecodeHexDigit(const char32_t character)
	{
		if(character >= '0' && character <= '9')
			return static_cast<u8_t>(character - '0');
		EL_ERROR(character < 'A' || character > 'F', TInvalidArgumentException, "hex digit", "invalid hexadecimal character");
		return static_cast<u8_t>(character - 'A' + 10);
	}

	void TELM327::AppendHexLine(TString& hex, const TString& line)
	{
		usys_t start = 0;
		while(start < line.Length() && IsAsciiWhitespace(line[start]))
			start++;
		if(start + 2 <= line.Length() && IsHexDigit(line[start]) && line[start + 1] == ':')
			start += 2;

		for(usys_t i = start; i < line.Length(); i++)
			if(IsHexDigit(line[i]))
				hex += line[i];
	}

	TList<u8_t> TELM327::ParseReadDataByIdentifierResponse(const TString& response, const u16_t did)
	{
		const TString text = response.Upper();
		if(text.Contains(TStringView(U"NO DATA")) || text.Contains(TStringView(U"STOPPED")) || text.Contains(TStringView(U"UNABLE TO CONNECT")))
			return {};
		EL_ERROR(text.Contains(TStringView(U"ERROR")) || text.Contains('?'), TException, TString::Format(U"ELM327 error for DID %04x: %q", static_cast<u32_t>(did), response));

		TString hex;
		TString line;
		for(const char32_t character : text.chars)
		{
			if(character == '\r' || character == '\n' || character == '>')
			{
				if(line.Length() > 0)
				{
					AppendHexLine(hex, line);
					line.chars.Clear(NEG1);
				}
			}
			else
			{
				line += character;
			}
		}
		if(line.Length() > 0)
			AppendHexLine(hex, line);

		const TString marker = TString::Format(U"62%04x", did).Upper();
		const usys_t marker_position = hex.Find(marker);
		if(marker_position == NEG1)
			return {};

		const usys_t data_position = marker_position + marker.Length();
		TList<u8_t> data((hex.Length() - data_position) / 2);
		for(usys_t i = data_position; i + 1 < hex.Length(); i += 2)
			data.Append(static_cast<u8_t>((DecodeHexDigit(hex[i]) << 4) | DecodeHexDigit(hex[i + 1])));
		return data;
	}

	TList<u8_t> TELM327::ReadDataByIdentifier(const u16_t did)
	{
		return ParseReadDataByIdentifierResponse(Command(TString::Format(U"22%04x", did).Upper()), did);
	}
}
