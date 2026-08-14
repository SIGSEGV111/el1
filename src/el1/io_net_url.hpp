#pragma once

#include "def.hpp"
#include "io_types.hpp"
#include "io_text_string.hpp"

namespace el1::io::net::url
{
	using namespace types;
	using namespace text::string;

	class TUrl
	{
		private:
			struct parsed_t
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
			};

			static parsed_t Parse(const TString& url);
			explicit TUrl(const parsed_t& parsed);

		public:
			const TString scheme;
			const TString user_info;
			const TString host;
			const u16_t port;
			const bool port_explicit;
			const bool has_authority;
			const TString path;
			const TString query;
			const TString fragment;

			explicit TUrl(const TString& url);

			TString RequestTarget() const;
			static u16_t DefaultPort(const TString& scheme);
	};
}
