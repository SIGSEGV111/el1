#pragma once

#include "io_types.hpp"

namespace el1::io::text::string
{
	class TString;
}

namespace el1::io::net::eth
{
	using namespace types;

	struct TMAC
	{
		byte_t octets[6];

		explicit operator text::string::TString() const EL_GETTER;

		TMAC();
		TMAC(const text::string::TString&);
	};
}
