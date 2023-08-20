#include "io_net_ip.hpp"

namespace el1::io::net::ip
{
	using namespace text::string;

	u32_t& ipaddr_t::IPv4()
	{
		EL_ERROR(!this->IsV4(), TException, TString::Format("%q is not an IPv4 address", this->operator TString()));
		return u32[3];
	}

	const u32_t& ipaddr_t::IPv4() const
	{
		EL_ERROR(!this->IsV4(), TException, TString::Format("%q is not an IPv4 address", this->operator TString()));
		return u32[3];
	}

	ipaddr_t& ipaddr_t::operator|=(const ipaddr_t& rhs)
	{
		EL_NOT_IMPLEMENTED;
	}

	ipaddr_t& ipaddr_t::operator&=(const ipaddr_t& rhs)
	{
		EL_NOT_IMPLEMENTED;
	}

	ipaddr_t ipaddr_t::operator|(const ipaddr_t& rhs) const
	{
		EL_NOT_IMPLEMENTED;
	}

	ipaddr_t ipaddr_t::operator&(const ipaddr_t& rhs) const
	{
		EL_NOT_IMPLEMENTED;
	}

	EIP ipaddr_t::Version() const
	{
		return (u64[0] == 0 && group[4] == 0 && group[5] == 0xffff) ? EIP::V4 : EIP::V6;
	}

	ipaddr_t::ipaddr_t(const u32_t ipv4)
	{
		u64[0] = 0;
		group[4] = 0;
		group[5] = 0xffff;
		u32[3] = ipv4;
	}

	ipaddr_t::ipaddr_t(const u8_t cidr, const EIP ver)
	{
		EL_NOT_IMPLEMENTED;
	}

	ipaddr_t::operator TString() const
	{
		if(IsV4())
		{
			return TString::Format("%d.%d.%d.%d",
				octet[12],
				octet[13],
				octet[14],
				octet[15]
			);
		}
		else
		{
			return TString::Format("%04x:%04x:%04x:%04x",
				group[0],
				group[1],
				group[2],
				group[3]
			);
		}
	}
}
