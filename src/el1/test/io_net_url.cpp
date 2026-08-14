#include <gtest/gtest.h>
#include <el1/io_net_url.hpp>

using namespace ::testing;

namespace
{
	using namespace el1::io::net::url;

	TEST(io_net_url, TUrl_absolute_http)
	{
		const TUrl url(el1::io::text::string::TString(U"https://user@example.com:8443/a/b?x=1#fragment"));
		EXPECT_EQ(url.scheme, U"https");
		EXPECT_EQ(url.user_info, U"user");
		EXPECT_EQ(url.host, U"example.com");
		EXPECT_EQ(url.port, 8443);
		EXPECT_TRUE(url.port_explicit);
		EXPECT_TRUE(url.has_authority);
		EXPECT_EQ(url.path, U"/a/b");
		EXPECT_EQ(url.query, U"x=1");
		EXPECT_EQ(url.fragment, U"fragment");
		EXPECT_EQ(url.RequestTarget(), U"/a/b?x=1");
	}

	TEST(io_net_url, TUrl_defaults_ipv6_and_relative)
	{
		const TUrl https(el1::io::text::string::TString(U"HTTPS://[2001:db8::1]/token"));
		EXPECT_EQ(https.scheme, U"https");
		EXPECT_EQ(https.host, U"2001:db8::1");
		EXPECT_EQ(https.port, 443);
		EXPECT_FALSE(https.port_explicit);

		const TUrl relative(el1::io::text::string::TString(U"/callback?code=abc&state=xyz"));
		EXPECT_EQ(relative.scheme, U"");
		EXPECT_FALSE(relative.has_authority);
		EXPECT_EQ(relative.path, U"/callback");
		EXPECT_EQ(relative.query, U"code=abc&state=xyz");
		EXPECT_EQ(relative.RequestTarget(), U"/callback?code=abc&state=xyz");
	}

	TEST(io_net_url, TUrl_rejects_invalid_syntax)
	{
		EXPECT_THROW(TUrl(el1::io::text::string::TString(U"https://example.com:70000/")), el1::error::TInvalidArgumentException);
		EXPECT_THROW(TUrl(el1::io::text::string::TString(U"https://2001:db8::1/")), el1::error::TInvalidArgumentException);
		EXPECT_THROW(TUrl(el1::io::text::string::TString(U"https://[2001:db8::1/token")), el1::error::TInvalidArgumentException);
		EXPECT_THROW(TUrl(el1::io::text::string::TString(U"1https://example.com/")), el1::error::TInvalidArgumentException);
	}

	TEST(io_net_url, TUrl_parser_handles_empty_components_and_network_path)
	{
		const TUrl network(el1::io::text::string::TString(U"//user@example.com:8080?"));
		EXPECT_TRUE(network.has_authority);
		EXPECT_EQ(network.user_info, U"user");
		EXPECT_EQ(network.host, U"example.com");
		EXPECT_EQ(network.port, 8080);
		EXPECT_EQ(network.path, U"/");
		EXPECT_EQ(network.query, U"");

		const TUrl relative(el1::io::text::string::TString(U"foo/bar:baz#"));
		EXPECT_FALSE(relative.has_authority);
		EXPECT_EQ(relative.path, U"foo/bar:baz");
		EXPECT_EQ(relative.fragment, U"");
	}
}
