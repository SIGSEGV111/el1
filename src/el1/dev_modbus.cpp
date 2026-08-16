#include "dev_modbus.hpp"
#include "util_bits.hpp"
#include "error.hpp"
#include "debug.hpp"

namespace el1::dev::modbus
{
	using namespace error;
	using namespace tty;
	using namespace util::bits;
	using namespace io::text;

	const char* ExceptionCodeToString(const EExceptionCode code)
	{
		switch(code)
		{
			case EExceptionCode::NONE                             : return "no error";
			case EExceptionCode::ILLEGAL_FUNCTION                 : return "ILLEGAL_FUNCTION";
			case EExceptionCode::ILLEGAL_DATA_ADDRESS             : return "ILLEGAL_DATA_ADDRESS";
			case EExceptionCode::ILLEGAL_DATA_VALUE               : return "ILLEGAL_DATA_VALUE";
			case EExceptionCode::SLAVE_DEVICE_FAILURE             : return "SLAVE_DEVICE_FAILURE";
			case EExceptionCode::ACKNOWLEDGE                      : return "ACKNOWLEDGE";
			case EExceptionCode::SLAVE_DEVICE_BUSY                : return "SLAVE_DEVICE_BUSY";
			case EExceptionCode::MEMORY_PARITY_ERROR              : return "MEMORY_PARITY_ERROR";
			case EExceptionCode::GATEWAY_PATH_UNAVAILABLE         : return "GATEWAY_PATH_UNAVAILABLE";
			case EExceptionCode::GATEWAY_TARGET_FAILED_TO_RESPOND : return "GATEWAY_TARGET_FAILED_TO_RESPOND";
		}

		return "non-standard exception code";
	}

	TString TSlaveException::Message() const
	{
		return TString::Format(U"Slave device %02x returned error code %02x (%s) in response to function %02x", device_id, (u8_t)code, ExceptionCodeToString(code), (u8_t)function);
	}

	IException* TSlaveException::Clone() const
	{
		return new TSlaveException(*this);
	}

	TSlaveException::TSlaveException(const EFunctionCode function, const EExceptionCode code, const u8_t device_id)
		: function(function), code(code), device_id(device_id)
	{
	}

	/////////////////////////////////////////////////////////////////////////////////////////////////////

	bool TFrameBuffer::operator==(const TFrameBuffer& rhs) const
	{
		if(this->pos_write != rhs.pos_write)
			return false;
		return memcmp(this->buffer, rhs.buffer, pos_write) == 0;
	}

	void TFrameBuffer::EnsureSpace(const unsigned n_need)
	{
		EL_ERROR(pos_write + n_need > MAX_FRAME_SIZE, TIndexOutOfBoundsException, 0, MAX_FRAME_SIZE - 1, pos_write + n_need);
	}

	void TFrameBuffer::WriteByte(const byte_t v)
	{
		EnsureSpace(1);
		buffer[pos_write++] = v;
	}

	void TFrameBuffer::WriteWord(const u16_t v)
	{
		WriteByte((v & 0xFF00) >> 8);
		WriteByte(v & 0x00FF);
	}

	void TFrameBuffer::WriteRequestHeader(frame_header_t h)
	{
		EL_ERROR(pos_write != 0, TLogicException);
		h.reg_addr = htobe16(h.reg_addr);
		memcpy(buffer, &h, sizeof(h));
		pos_write += sizeof(h);
	}

	void TFrameBuffer::WriteArray(const unsigned n, const u16_t* const arr)
	{
		const unsigned sz_bytes = n * 2;
		EL_ERROR(sz_bytes > MAX_ARRAY_SIZE, TInvalidArgumentException, "arr", "array is too large");
		EL_ERROR(n > 0 && arr == nullptr, TInvalidArgumentException, "arr", "array must not be null");
		EnsureSpace(sz_bytes + 1);
		WriteByte(sz_bytes);
		for(unsigned i = 0; i < n; i++)
			WriteWord(arr[i]);
	}

	void TFrameBuffer::WriteArray(const unsigned n, const bool* const arr)
	{
		const unsigned sz_bytes = (n + 7) / 8;
		EL_ERROR(sz_bytes > MAX_ARRAY_SIZE, TInvalidArgumentException, "arr", "array is too large");
		EL_ERROR(n > 0 && arr == nullptr, TInvalidArgumentException, "arr", "array must not be null");
		EnsureSpace(sz_bytes + 1);
		WriteByte(sz_bytes);
		memset(buffer + pos_write, 0, sz_bytes);
		for(unsigned i = 0; i < n; i++)
			SetBit(buffer + pos_write, i, arr[i]);
		pos_write += sz_bytes;
	}

	u16_t TFrameBuffer::ComputeCRC(const unsigned n, const byte_t* const arr)
	{
		u16_t crc = 0xFFFFu;

		for(unsigned i = 0; i < n; ++i)
		{
			crc ^= static_cast<u16_t>(arr[i]);

			for(unsigned bit = 0; bit < 8; ++bit)
			{
				if(crc & 0x0001u)
				{
					crc >>= 1;
					crc ^= 0xA001u;
				}
				else
				{
					crc >>= 1;
				}
			}
		}

		return crc;
	}

	void TFrameBuffer::WriteCRC()
	{
		const u16_t crc = ComputeCRC(pos_write, buffer);
		WriteByte(crc & 0x00FF);
		WriteByte((crc & 0xFF00) >> 8);
	}

	void TFrameBuffer::VerifyCRC()
	{
		EL_ERROR(pos_write < 3, TException, U"response frame too short");
		const u16_t computed_crc = ComputeCRC(pos_write - 2, buffer);
		const u16_t expected_crc = (u16_t)buffer[pos_write - 2] | ((u16_t)buffer[pos_write - 1] << 8);
		EL_ERROR(computed_crc != expected_crc, TException, TString::Format(U"CRC does not match (expected: %04x, computed %04x)", expected_crc, computed_crc));
	}

	void TFrameBuffer::EnsureRemaining(const unsigned n_want)
	{
		EL_ERROR(pos_read + n_want > pos_write, TIndexOutOfBoundsException, 0, pos_write - 1, pos_read + n_want);
	}

	byte_t TFrameBuffer::ReadByte()
	{
		EnsureRemaining(1);
		return buffer[pos_read++];
	}

	u16_t TFrameBuffer::ReadWord()
	{
		return (u16_t)(ReadByte() << 8) | (u16_t)ReadByte();
	}

	void TFrameBuffer::ReadByteExpected(const char* const field_name, const byte_t expected_value)
	{
		const byte_t actual_value = ReadByte();
		EL_ERROR(actual_value != expected_value, TException, TString::Format(U"expected byte-field %q to have value %02x, but got %02x", field_name, expected_value, actual_value));
	}

	void TFrameBuffer::ReadWordExpected(const char* const field_name, const u16_t expected_value)
	{
		const u16_t actual_value = ReadWord();
		EL_ERROR(actual_value != expected_value, TException, TString::Format(U"expected word-field %q to have value %04x, but got %04x", field_name, expected_value, actual_value));
	}

	void TFrameBuffer::ReadArray(const u16_t n_expected, bool* const arr)
	{
		const unsigned sz_arr = (n_expected + 7) / 8;
		ReadByteExpected("array_size", sz_arr);
		EnsureRemaining(sz_arr);
		for(unsigned i = 0; i < n_expected; i++)
			arr[i] = GetBit(buffer + pos_read, i);
		pos_read += sz_arr;
	}

	void TFrameBuffer::ReadArray(const u16_t n_expected, u16_t* const arr)
	{
		const unsigned sz_arr = n_expected * 2;
		ReadByteExpected("array_size", sz_arr);
		EnsureRemaining(sz_arr);
		for(unsigned i = 0; i < n_expected; i++)
			arr[i] = ReadWord();
	}

	void TFrameBuffer::SleepFrameGap(tty::TTeletypewriter& tty, TTime& ts_eof) const
	{
		const double baudrate = tty.Baudrate();
		EL_ERROR(baudrate <= 0.0, TException, U"TTY baud rate must be greater than zero");
		const TConfiguration config = tty.Config();
		const double bits_per_symbol = 1 + (u32_t)config.data_bits + ((config.parity != EParity::NONE) ? 1 : 0) + (u32_t)config.stop_bits;
		const double time_per_symbol = bits_per_symbol / baudrate;
		const TTime symbol_timeout = time_per_symbol * 3.5;
		const TTime t_next_frame = ts_eof + symbol_timeout;
		const TTime now = TTime::Now(EClock::MONOTONIC);
		if(t_next_frame > now)
			system::task::TFiber::Sleep(t_next_frame - now);
	}

	void TFrameBuffer::Send(TTeletypewriter& tty, const bool tx_echo, TTime& ts_eof) const
	{
		SleepFrameGap(tty, ts_eof);
		if(TModBus::DEBUG) debug::Hexdump("TX    ", buffer, pos_write);
		tty.WriteAll(buffer, pos_write);
		tty.TxSync();
		ts_eof = TTime::Now(EClock::MONOTONIC);
		if(tx_echo)
		{
			TFrameBuffer loopback;
			loopback.ReceiveExcact(tty, pos_write);
			EL_ERROR(*this != loopback, TException, U"TX data corruption/collision; loopback does not match what we intended to send");
		}
	}

	void TFrameBuffer::ReceiveExcact(TTeletypewriter& tty, const u16_t n_bytes)
	{
		EL_ERROR(tty.BlockingRead(buffer, n_bytes, 0.1) != n_bytes, TException, U"BlockingRead() did not return the expected amount of data");
		if(TModBus::DEBUG) debug::Hexdump("RX(RE)", buffer, n_bytes);
		pos_write = n_bytes;
	}

	u16_t TFrameBuffer::Space() const
	{
		return MAX_FRAME_SIZE - pos_write;
	}

	void TFrameBuffer::ReceiveResponse(TTeletypewriter& tty, const TTime response_timeout, const u8_t device_id, EFunctionCode function, TTime& ts_eof)
	{
		const double baudrate = tty.Baudrate();
		EL_ERROR(baudrate <= 0.0, TException, U"TTY baud rate must be greater than zero");
		const TConfiguration config = tty.Config();
		const double bits_per_symbol = 1 + (u32_t)config.data_bits + ((config.parity != EParity::NONE) ? 1 : 0) + (u32_t)config.stop_bits;
		const double time_per_symbol = bits_per_symbol / baudrate;
		const TTime symbol_timeout = time_per_symbol * 3.5 * 5;

		EL_ERROR(tty.BlockingRead(buffer, 1, response_timeout + time_per_symbol) != 1, TException, U"timeout waiting for response");
		pos_write = 1;

		for(;;)
		{
			EL_ERROR(Space() == 0, TException, U"received Modbus frame exceeds buffer size");
			const usys_t n_read = tty.BlockingRead(buffer + pos_write, Space(), symbol_timeout);
			if(n_read == 0)
				break;
			pos_write += n_read;
		}

		if(TModBus::DEBUG) debug::Hexdump("RX(RR)", buffer, pos_write);

		EL_ERROR(pos_write < 4, TException, U"received frame is too short");
		VerifyCRC();

		ReadByteExpected("device_id", device_id);
		const u8_t status_code = ReadByte();
		EL_ERROR((status_code & 0x7F) != (u8_t)function, TException, TString::Format(U"unexpected function code in response from slave (expected %02x; got %02x)", (u8_t)function, status_code & 0x7F));
		EL_ERROR(status_code & 0x80, TSlaveException, function, (EExceptionCode)ReadByte(), device_id);
	}

	TFrameBuffer::TFrameBuffer() : pos_write(0), pos_read(0) {}

	/////////////////////////////////////////////////////////////////////////////////////////////////////

	bool TDevice::ReadCoil(const u16_t address) const
	{
		return bus->ReadCoil(id, address);
	}

	void TDevice::ReadCoils(const u16_t start_address, const u16_t n_coils, bool* const arr_state) const
	{
		return bus->ReadCoils(id, start_address, n_coils, arr_state);
	}

	void TDevice::WriteCoil(const u16_t address, const bool new_state)
	{
		bus->WriteCoil(id, address, new_state);
	}

	void TDevice::WriteCoils(const u16_t start_address, const u16_t n_coils, const bool* const arr_state)
	{
		bus->WriteCoils(id, start_address, n_coils, arr_state);
	}

	bool TDevice::ReadDiscreteInput(const u16_t address) const
	{
		return bus->ReadDiscreteInput(id, address);
	}

	void TDevice::ReadDiscreteInputs(const u16_t start_address, const u16_t n_registers, bool* const arr_state) const
	{
		bus->ReadDiscreteInputs(id, start_address, n_registers, arr_state);
	}

	u16_t TDevice::ReadHoldingRegister(const u16_t address) const
	{
		return bus->ReadHoldingRegister(id, address);
	}

	void TDevice::ReadHoldingRegisters(const u16_t start_address, const u16_t n_registers, u16_t* const arr_values) const
	{
		bus->ReadHoldingRegisters(id, start_address, n_registers, arr_values);
	}

	void TDevice::WriteHoldingRegister(const u16_t address, const u16_t new_value)
	{
		bus->WriteHoldingRegister(id, address, new_value);
	}

	void TDevice::WriteHoldingRegisters(const u16_t start_address, const u16_t n_registers, const u16_t* const arr_values)
	{
		bus->WriteHoldingRegisters(id, start_address, n_registers, arr_values);
	}

	u16_t TDevice::ReadInputRegister(const u16_t address) const
	{
		return bus->ReadInputRegister(id, address);
	}

	void TDevice::ReadInputRegisters(const u16_t start_address, const u16_t n_registers, u16_t* const arr_values) const
	{
		return bus->ReadInputRegisters(id, start_address, n_registers, arr_values);
	}

	/////////////////////////////////////////////////////////////////////////////////////////////////////

	bool TModBus::DEBUG = false;

	bool TModBus::ReadCoil(const u8_t device_id, const u16_t address) const
	{
		try
		{
			bool tmp;
			ReadCoils(device_id, address, 1, &tmp);
			return tmp;
		}
		catch(const IException& e)
		{
			EL_FORWARD(e, TException, TString::Format(U"during ReadCoil(device_id=%02x, address=%04x)", device_id, address));
		}
	}

	void TModBus::ReadCoils(const u8_t device_id, const u16_t start_address, const u16_t n_coils, bool* const arr_state) const
	{
		try
		{
			TFrameBuffer request;
			EL_ERROR(n_coils == 0 || n_coils > 2000, TInvalidArgumentException, "n_coils", "Modbus allows 1..2000 coils per read request");
			EL_ERROR(arr_state == nullptr, TInvalidArgumentException, "arr_state", "output array must not be null");
			request.WriteRequestHeader({ .device_id = device_id, .function = EFunctionCode::READ_COILS, .reg_addr = start_address });
			request.WriteWord(n_coils);
			request.WriteCRC();
			request.Send(*tty, tx_echo, ts_eof);

			TFrameBuffer response;
			response.ReceiveResponse(*tty, response_timeout, device_id, EFunctionCode::READ_COILS, ts_eof);
			response.ReadArray(n_coils, arr_state);
		}
		catch(const IException& e)
		{
			EL_FORWARD(e, TException, TString::Format(U"during ReadCoils(device_id=%02x, start_address=%04x, n_coils=%d)", device_id, start_address, n_coils));
		}
	}

	void TModBus::WriteCoil(const u8_t device_id, const u16_t address, const bool new_state)
	{
		try
		{
			TFrameBuffer request;
			request.WriteRequestHeader({ .device_id = device_id, .function = EFunctionCode::WRITE_SINGLE_COIL, .reg_addr = address });
			request.WriteByte(new_state ? 0xFF : 0x00);
			request.WriteByte(0x00);
			request.WriteCRC();
			request.Send(*tty, tx_echo, ts_eof);

			TFrameBuffer response;
			response.ReceiveResponse(*tty, response_timeout, device_id, EFunctionCode::WRITE_SINGLE_COIL, ts_eof);
			if(!lax_response)
			{
				response.ReadByteExpected("new_state", new_state ? 0xFF : 0x00);
				response.ReadByteExpected("zero_byte", 0x00);
			}
		}
		catch(const IException& e)
		{
			EL_FORWARD(e, TException, TString::Format(U"during WriteCoil(device_id=%02x, address=%04x, new_state=%d)", device_id, address, new_state));
		}
	}

	void TModBus::WriteCoils(const u8_t device_id, const u16_t start_address, const u16_t n_coils, const bool* const arr_state)
	{
		try
		{
			EL_ERROR(n_coils == 0 || n_coils > 1968, TInvalidArgumentException, "n_coils", "Modbus allows 1..1968 coils per write request");
			EL_ERROR(arr_state == nullptr, TInvalidArgumentException, "arr_state", "input array must not be null");
			TFrameBuffer request;
			request.WriteRequestHeader({ .device_id = device_id, .function = EFunctionCode::WRITE_MULTIPLE_COILS, .reg_addr = start_address });
			request.WriteWord(n_coils);
			request.WriteArray(n_coils, arr_state);
			request.WriteCRC();
			request.Send(*tty, tx_echo, ts_eof);

			TFrameBuffer response;
			response.ReceiveResponse(*tty, response_timeout, device_id, EFunctionCode::WRITE_MULTIPLE_COILS, ts_eof);
			if(!lax_response)
			{
				response.ReadWordExpected("start_address", start_address);
				response.ReadWordExpected("n_coils", n_coils);
			}
		}
		catch(const IException& e)
		{
			EL_FORWARD(e, TException, TString::Format(U"during WriteCoils(device_id=%02x, start_address=%04x, n_coils=%d)", device_id, start_address, n_coils));
		}
	}

	bool TModBus::ReadDiscreteInput(const u8_t device_id, const u16_t address) const
	{
		try
		{
			bool tmp;
			ReadDiscreteInputs(device_id, address, 1, &tmp);
			return tmp;
		}
		catch(const IException& e)
		{
			EL_FORWARD(e, TException, TString::Format(U"during ReadDiscreteInput(device_id=%02x, address=%04x)", device_id, address));
		}
	}

	void TModBus::ReadDiscreteInputs(const u8_t device_id, const u16_t start_address, const u16_t n_registers, bool* const arr_state) const
	{
		try
		{
			EL_ERROR(n_registers == 0 || n_registers > 2000, TInvalidArgumentException, "n_registers", "Modbus allows 1..2000 discrete inputs per read request");
			EL_ERROR(arr_state == nullptr, TInvalidArgumentException, "arr_state", "output array must not be null");
			TFrameBuffer request;
			request.WriteRequestHeader({ .device_id = device_id, .function = EFunctionCode::READ_DISCRETE_INPUTS, .reg_addr = start_address });
			request.WriteWord(n_registers);
			request.WriteCRC();
			request.Send(*tty, tx_echo, ts_eof);

			TFrameBuffer response;
			response.ReceiveResponse(*tty, response_timeout, device_id, EFunctionCode::READ_DISCRETE_INPUTS, ts_eof);
			response.ReadArray(n_registers, arr_state);
		}
		catch(const IException& e)
		{
			EL_FORWARD(e, TException, TString::Format(U"during ReadDiscreteInputs(device_id=%02x, start_address=%04x, n_registers=%d)", device_id, start_address, n_registers));
		}
	}

	u16_t TModBus::ReadHoldingRegister(const u8_t device_id, const u16_t address) const
	{
		try
		{
			u16_t tmp;
			ReadHoldingRegisters(device_id, address, 1, &tmp);
			return tmp;
		}
		catch(const IException& e)
		{
			EL_FORWARD(e, TException, TString::Format(U"during ReadHoldingRegister(device_id=%02x, address=%04x)", device_id, address));
		}
	}

	void TModBus::ReadHoldingRegisters(const u8_t device_id, const u16_t start_address, const u16_t n_registers, u16_t* const arr_values) const
	{
		try
		{
			EL_ERROR(n_registers == 0 || n_registers > 125, TInvalidArgumentException, "n_registers", "Modbus allows 1..125 holding registers per read request");
			EL_ERROR(arr_values == nullptr, TInvalidArgumentException, "arr_values", "output array must not be null");
			TFrameBuffer request;
			request.WriteRequestHeader({ .device_id = device_id, .function = EFunctionCode::READ_HOLDING_REGISTERS, .reg_addr = start_address });
			request.WriteWord(n_registers);
			request.WriteCRC();
			request.Send(*tty, tx_echo, ts_eof);

			TFrameBuffer response;
			response.ReceiveResponse(*tty, response_timeout, device_id, EFunctionCode::READ_HOLDING_REGISTERS, ts_eof);
			response.ReadArray(n_registers, arr_values);
		}
		catch(const IException& e)
		{
			EL_FORWARD(e, TException, TString::Format(U"during ReadHoldingRegisters(device_id=%02x, start_address=%04x, n_registers=%d)", device_id, start_address, n_registers));
		}
	}

	void TModBus::WriteHoldingRegister(const u8_t device_id, const u16_t address, const u16_t new_value)
	{
		try
		{
			TFrameBuffer request;
			request.WriteRequestHeader({ .device_id = device_id, .function = EFunctionCode::WRITE_SINGLE_REGISTER, .reg_addr = address });
			request.WriteWord(new_value);
			request.WriteCRC();
			request.Send(*tty, tx_echo, ts_eof);

			TFrameBuffer response;
			response.ReceiveResponse(*tty, response_timeout, device_id, EFunctionCode::WRITE_SINGLE_REGISTER, ts_eof);
			if(!lax_response)
			{
				response.ReadWordExpected("address", address);
				response.ReadWordExpected("new_value", new_value);
			}
		}
		catch(const IException& e)
		{
			EL_FORWARD(e, TException, TString::Format(U"during WriteHoldingRegister(device_id=%02x, address=%04x, new_value=%04x, lax_response=%d)", device_id, address, new_value, lax_response));
		}
	}

	void TModBus::WriteHoldingRegisters(const u8_t device_id, const u16_t start_address, const u16_t n_registers, const u16_t* const arr_values)
	{
		try
		{
			EL_ERROR(n_registers == 0 || n_registers > 123, TInvalidArgumentException, "n_registers", "Modbus allows 1..123 holding registers per write request");
			EL_ERROR(arr_values == nullptr, TInvalidArgumentException, "arr_values", "input array must not be null");
			TFrameBuffer request;
			request.WriteRequestHeader({ .device_id = device_id, .function = EFunctionCode::WRITE_MULTIPLE_REGISTERS, .reg_addr = start_address });
			request.WriteWord(n_registers);
			request.WriteArray(n_registers, arr_values);
			request.WriteCRC();
			request.Send(*tty, tx_echo, ts_eof);

			TFrameBuffer response;
			response.ReceiveResponse(*tty, response_timeout, device_id, EFunctionCode::WRITE_MULTIPLE_REGISTERS, ts_eof);
			if(!lax_response)
			{
				response.ReadWordExpected("start_address", start_address);
				response.ReadWordExpected("n_registers", n_registers);
			}
		}
		catch(const IException& e)
		{
			EL_FORWARD(e, TException, TString::Format(U"during WriteHoldingRegisters(device_id=%02x, start_address=%04x, n_registers=%d)", device_id, start_address, n_registers));
		}
	}

	u16_t TModBus::ReadInputRegister(const u8_t device_id, const u16_t address) const
	{
		try
		{
			u16_t tmp;
			ReadInputRegisters(device_id, address, 1, &tmp);
			return tmp;
		}
		catch(const IException& e)
		{
			EL_FORWARD(e, TException, TString::Format(U"during ReadInputRegister(device_id=%02x, address=%04x)", device_id, address));
		}
	}

	void TModBus::ReadInputRegisters(const u8_t device_id, const u16_t start_address, const u16_t n_registers, u16_t* const arr_values) const
	{
		try
		{
			EL_ERROR(n_registers == 0 || n_registers > 125, TInvalidArgumentException, "n_registers", "Modbus allows 1..125 input registers per read request");
			EL_ERROR(arr_values == nullptr, TInvalidArgumentException, "arr_values", "output array must not be null");
			TFrameBuffer request;
			request.WriteRequestHeader({ .device_id = device_id, .function = EFunctionCode::READ_INPUT_REGISTERS, .reg_addr = start_address });
			request.WriteWord(n_registers);
			request.WriteCRC();
			request.Send(*tty, tx_echo, ts_eof);

			TFrameBuffer response;
			response.ReceiveResponse(*tty, response_timeout, device_id, EFunctionCode::READ_INPUT_REGISTERS, ts_eof);
			response.ReadArray(n_registers, arr_values);
		}
		catch(const IException& e)
		{
			EL_FORWARD(e, TException, TString::Format(U"during ReadInputRegisters(device_id=%02x, start_address=%04x, n_registers=%d)", device_id, start_address, n_registers));
		}
	}

	std::unique_ptr<TDevice> TModBus::ClaimDevice(const u8_t device_id)
	{
		return New<TDevice>(this, device_id);
	}

	TModBus::TModBus(tty::TTeletypewriter* const tty, const TTime response_timeout, const bool tx_echo)
		: tty(tty), response_timeout(response_timeout), tx_echo(tx_echo), lax_response(false)
	{
		EL_ERROR(tty == nullptr, TInvalidArgumentException, "tty", "TTY must not be null");
		EL_ERROR(response_timeout < 0, TInvalidArgumentException, "response_timeout", "response timeout must not be negative");
	}
}
