#include "io_net_url.hpp"
#include "io_text_parser.hpp"

namespace el1::io::net::url
{
	using namespace error;
	using namespace text::parser;

	namespace
	{
		constexpr char32_t UNICODE_MAX = (char32_t)0x10ffff;

		static TUrl::TAuthority MakeAuthority(const std::optional<TStringView> username, const TStringView host, const std::optional<TStringView> port)
		{
			const s64_t n_port = port ? port->ToInteger() : 0;
			EL_ERROR((port && n_port < 1) || n_port > 65535, TException, "invalid port");
			return {{}, username.value_or(U""), host, (u16_t)n_port};
		}

		static auto MakeParser()
		{
			auto any = CharRange((char32_t)0, UNICODE_MAX);
			auto url_char = ~CharList(U'?', U'#');
			auto alpha = CharRange(U'A', U'Z') || CharRange(U'a', U'z');
			auto scheme_char = alpha || CharRange(U'0', U'9') || CharList(U'+', U'-', U'.');

			auto scheme = Capture(alpha + Repeat(0, 31, scheme_char)) + Discard(U"://"_P);
			auto username = Capture(Repeat(0, 256, ~CharList(U'@', U'/', U'?', U'#'))) + Discard(U'@'_P);
			auto bracketed_host = Between(U'['_P, Capture(Repeat(1, 256, ~CharList(U']', U'/', U'?', U'#'))), U']'_P);
			auto host = bracketed_host || Capture(Repeat(1, 256, ~CharList(U':', U'@', U'/', U'?', U'#')));
			auto port = Discard(U':'_P) + Capture(Repeat(1, 5, CharRange(U'0', U'9')));
			auto authority_end = LookAhead(Discard(CharList(U'/', U'?', U'#')) || End());
			auto authority_body = Translate([](auto u, auto h, auto p) { return MakeAuthority(u, h, p); }, Maybe(username), host, Maybe(port)) + authority_end;
			auto absolute = Translate([](auto s, auto a) { a.scheme = s; a.scheme.ToLower(); return a; }, scheme, Expect(authority_body));
			auto network = Discard(U"//"_P) + Expect(authority_body);

			auto path = Capture(Repeat(0, 1024, url_char));
			auto query = Discard(U'?'_P) + Capture(Repeat(0, 1024, ~CharList(U'#')));
			auto fragment = Discard(U'#'_P) + Capture(Repeat(0, 256, any));

			return Translate(
				[](auto authority, auto path, auto query, auto fragment)
				{
					if(!authority)
						for(usys_t i = 0; i < path.Length() && path[i] != U'/'; i++)
							EL_ERROR(path[i] == U':', TException, "invalid relative URL");

					TUrl url{authority.value_or(TUrl::TAuthority{}), path, query.value_or(U""), fragment.value_or(U"")};
					if(authority && url.path.Length() == 0)
						url.path = TString(U"/");
					return url;
				},
				Maybe(absolute || network), path, Maybe(query), Maybe(fragment)
			);
		}
	}

	u16_t TUrl::DefaultPort(const TString& scheme)
	{
		if(scheme == U"http")
			return 80;
		if(scheme == U"https")
			return 443;
		return 0;
	}

	TUrl TUrl::FromString(const TStringView text)
	{
		return EL_ANNOTATE_ERROR(MakeParser().Parse(text), TInvalidArgumentException, "text", "invalid URL");
	}

	TString TUrl::RequestTarget() const
	{
		TString result = path.Length() == 0 ? TString(U"/") : path;
		if(query.Length() > 0)
			result += TString(U"?") + query;
		return result;
	}
}
