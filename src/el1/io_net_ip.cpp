#include "io_net_ip.hpp"
#include "util_bits.hpp"

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
		u64[0] |= rhs.u64[0];
		u64[1] |= rhs.u64[1];
		return *this;
	}

	ipaddr_t& ipaddr_t::operator&=(const ipaddr_t& rhs)
	{
		u64[0] &= rhs.u64[0];
		u64[1] &= rhs.u64[1];
		return *this;
	}

	ipaddr_t ipaddr_t::operator|(const ipaddr_t& rhs) const
	{
		ipaddr_t a = *this;
		a |= rhs;
		return a;
	}

	ipaddr_t ipaddr_t::operator&(const ipaddr_t& rhs) const
	{
		ipaddr_t a = *this;
		a &= rhs;
		return a;
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

	ipaddr_t ipaddr_t::MaskFromCidr(const u8_t cidr, const EIP ver)
	{
		ipaddr_t a;
		a.u64[0] = 0;
		a.u64[1] = 0;

		const unsigned n_bits = (ver == EIP::V4 ? 128-32 : 0) + cidr;
		EL_ERROR(n_bits > 128, TInvalidArgumentException, "cidr", "cidr is invalid");

		for(unsigned i = 0; i < n_bits; i++)
			util::bits::SetBit(a.octet, i, true);

		return a;
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
			return TString::Format("%04x:%04x:%04x:%04x:%04x:%04x:%04x:%04x",
				group[0],
				group[1],
				group[2],
				group[3],
				group[4],
				group[5],
				group[6],
				group[7]
			);
		}
	}

	bool netmask_t::Matches(const ipaddr_t& other_ip) const
	{
		const ipaddr_t mask = ipaddr_t::MaskFromCidr(cidr, ip.Version());

		const ipaddr_t a = (other_ip & mask);
		const ipaddr_t b = (this->ip & mask);

		// std::cerr<<"this->ip = "<<(TString)this->ip<<std::endl;
		// std::cerr<<"other_ip = "<<(TString)other_ip<<std::endl;
		// std::cerr<<"mask     = "<<(TString)mask<<std::endl;
		// std::cerr<<"a        = "<<(TString)a<<std::endl;
		// std::cerr<<"b        = "<<(TString)b<<std::endl;

		return a == b;
	}

	netmask_t::netmask_t(const text::string::TString& str, const EIP version)
	{
		if(str.Contains("/"))
		{
			auto arr = str.Split("/", 2);
			ip = ipaddr_t(arr[0], version);
			cidr = arr[1].ToInteger();
		}
		else
		{
			ip = ipaddr_t(str, version);
			cidr = ip.IsV4() ? 32 : 128;
		}
	}
}
