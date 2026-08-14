#pragma once

#include "def.hpp"
#include "io_types.hpp"
#include "io_text_string.hpp"

namespace el1::io::net::url
{
	using namespace types;
	using namespace text::string;

	struct TUrl
	{
		struct TAuthority
		{
			TString scheme;
			TString username;
			TString host;
			u16_t port = 0;
		};

		TAuthority authority;
		TString path;
		TString query;
		TString fragment;

		static TUrl FromString(TStringView text);
		static u16_t DefaultPort(const TString& scheme);
		TString RequestTarget() const;
	};
}
