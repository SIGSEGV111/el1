#include "dev_w1.hpp"
#include "io_text_string.hpp"
#include <string.h>

namespace el1::dev::w1
{
	using namespace error;
	using namespace io::text::string;

	const uuid_t uuid_t::NULL_VALUE = { 0, { 0, 0, 0, 0, 0, 0 } };

	TString TCrcMismatchException::Message() const
	{
		return TString::Format(U"1-wire data transfer checksum mismatch (rom = %s, received = %02x, calculated = %02x)", uuid.ToString(), received_crc, calculated_crc);
	}

	IException* TCrcMismatchException::Clone() const
	{
		return new TCrcMismatchException(*this);
	}

	byte_t& uuid_t::Octet(const unsigned idx_byte)
	{
		return reinterpret_cast<byte_t*>(this)[idx_byte];
	}

	const byte_t& uuid_t::Octet(const unsigned idx_byte) const
	{
		return reinterpret_cast<const byte_t*>(this)[idx_byte];
	}

	void uuid_t::Bit(const unsigned idx_bit, const bool new_state)
	{
		const byte_t mask1 = (1 << (idx_bit % 8U));
		const byte_t mask2 = ~mask1;
		byte_t& b = this->Octet(idx_bit / 8U);
		b = (b & mask2) | (new_state ? mask1 : 0);
	}

	bool uuid_t::Bit(const unsigned idx_bit) const
	{
		const byte_t mask1 = (1 << (idx_bit % 8U));
		const byte_t& b = this->Octet(idx_bit / 8U);
		return (b & mask1) != 0;
	}

	TString uuid_t::ToString() const
	{
		return TString::Format(U"%02x|%02x:%02x:%02x:%02x:%02x:%02x|%02x",
			this->type,
			this->serial.octet[0],
			this->serial.octet[1],
			this->serial.octet[2],
			this->serial.octet[3],
			this->serial.octet[4],
			this->serial.octet[5],
			CalculateCRC(this, 7)
		);
	}

	uuid_t uuid_t::FromInt(const u64_t s)
	{
		uuid_t v;
		memcpy(&v, &s, 7);
		return v;
	}

	uuid_t uuid_t::FromString(const TStringView text)
	{
		static const usys_t OCTET_POSITIONS[8] = { 0, 3, 6, 9, 12, 15, 18, 21 };
		EL_ERROR(text.Length() != 23 || text[2] != '|' || text[5] != ':' || text[8] != ':'
			|| text[11] != ':' || text[14] != ':' || text[17] != ':' || text[20] != '|',
			TInvalidArgumentException, "text", "invalid 1-wire ROM; expected xx|xx:xx:xx:xx:xx:xx|xx");

		const auto parse_hex_nibble = [](const u32_t code) -> u8_t
		{
			if(code >= '0' && code <= '9')
				return static_cast<u8_t>(code - '0');
			if(code >= 'a' && code <= 'f')
				return static_cast<u8_t>(code - 'a' + 10);
			if(code >= 'A' && code <= 'F')
				return static_cast<u8_t>(code - 'A' + 10);
			EL_THROW(TInvalidArgumentException, "text", "1-wire ROM contains a non-hexadecimal digit");
		};

		u8_t values[8];
		for(usys_t i = 0; i < sizeof(values); i++)
		{
			const usys_t pos = OCTET_POSITIONS[i];
			values[i] = static_cast<u8_t>((parse_hex_nibble(text[pos]) << 4) | parse_hex_nibble(text[pos + 1]));
		}

		uuid_t uuid;
		for(usys_t i = 0; i < sizeof(uuid); i++)
			uuid.Octet(i) = values[i];
		const u8_t crc = CalculateCRC(&uuid, sizeof(uuid));
		EL_ERROR(crc != values[7], TCrcMismatchException, uuid, values[7], crc);
		return uuid;
	}


	u8_t CalculateCRC(const void* const arr_data, const unsigned n_data)
	{
		u8_t crc = 0;

		for(unsigned i = 0; i < n_data; i++)
		{
			u8_t b = reinterpret_cast<const u8_t*>(arr_data)[i];

			for(unsigned j = 0; j < 8; j++)
			{
				const u8_t m = (crc ^ b) & 0x01;
				crc >>= 1;

				if(m != 0)
				{
					crc ^= 0x8C;
				}

				b >>= 1;
			}
		}

		return crc;
	}
}
