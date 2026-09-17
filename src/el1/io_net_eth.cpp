#include "io_net_eth.hpp"
#include "io_text_string.hpp"
#include "io_text_number.hpp"
#include "error.hpp"

namespace el1::io::net::eth
{
	TMAC::operator text::string::TString() const
	{
		return text::string::TString::Format(U"%02x:%02x:%02x:%02x:%02x:%02x", octets[0], octets[1], octets[2], octets[3], octets[4], octets[5]);
	}

	TMAC::TMAC()
	{
		memset(octets, 0, sizeof(octets));
	}

	TMAC::TMAC(const text::string::TStringView str)
	{
		EL_ERROR(str.Length() != 17 || str[2] != U':' || str[5] != U':' || str[8] != U':' || str[11] != U':' || str[14] != U':',
			TException, TString::Format(U"invalid MAC-address %q", str));

		for(usys_t i = 0; i < 6; i++)
		{
			const auto value = text::number::TryParseHex<byte_t>(str.SliceSL(i * 3, 2));
			EL_ERROR(!value.has_value(), TException, TString::Format(U"invalid MAC-address %q", str));
			octets[i] = *value;
		}
	}
}
