#include "io_net_eth.hpp"
#include "io_text_string.hpp"
#include "error.hpp"
#include <stdio.h>

namespace el1::io::net::eth
{
	TMAC::operator text::string::TString() const
	{
		return text::string::TString::Format("%02x:%02x:%02x:%02x:%02x:%02x", octets[0], octets[1], octets[2], octets[3], octets[4], octets[5]);
	}

	TMAC::TMAC()
	{
		memset(octets, 0, sizeof(octets));
	}

	TMAC::TMAC(const text::string::TString& str)
	{
		EL_ERROR(sscanf(str.MakeCStr().get(), "%hhx:%hhx:%hhx:%hhx:%hhx:%hhx", octets + 0, octets + 1, octets + 2, octets + 3, octets + 4, octets + 5) != 6, TException, TString::Format("invalid MAC-address %q", str));
	}
}
