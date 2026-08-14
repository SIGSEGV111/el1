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
		TString scheme;
		TString user_info;
		TString host;
		u16_t port = 0;
		bool port_explicit = false;
		bool has_authority = false;
		TString path;
		TString query;
		TString fragment;

		static TUrl FromString(TStringView text);
		static u16_t DefaultPort(const TString& scheme);
		TString RequestTarget() const;
	};
}
