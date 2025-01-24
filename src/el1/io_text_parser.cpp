#include "io_text_parser.hpp"
#include "error.hpp"

namespace el1::io::text::parser
{
	TParser<ast::TLiteralNode> operator""_P(const char* const str, size_t len)
	{
		return TParser(ast::TLiteralNode(TString(str, len)));
	}
}
