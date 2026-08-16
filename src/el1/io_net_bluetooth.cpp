#include "io_net_bluetooth.hpp"
#include "error.hpp"
#include <cstdio>
#include <cstring>

namespace el1::io::net::bluetooth
{
	using namespace error;
	using namespace text::string;

	address_t::address_t() : octet{}
	{
	}

	address_t::address_t(const TStringView address) : octet{}
	{
		auto c_str = address.MakeCStr();
		u32_t parsed[6] = {};
		int consumed = 0;
		const int count = ::sscanf(
			c_str.get(),
			"%2x:%2x:%2x:%2x:%2x:%2x%n",
			&parsed[0], &parsed[1], &parsed[2], &parsed[3], &parsed[4], &parsed[5], &consumed
		);
		EL_ERROR(count != 6 || consumed != static_cast<int>(::strlen(c_str.get())), TInvalidArgumentException, "address", "must use XX:XX:XX:XX:XX:XX format");
		for(usys_t i = 0; i < 6; i++)
			octet[i] = static_cast<byte_t>(parsed[i]);
	}

	address_t::operator TString() const
	{
		return TString::Format(
			U"%02x:%02x:%02x:%02x:%02x:%02x",
			octet[0], octet[1], octet[2], octet[3], octet[4], octet[5]
		);
	}
}
