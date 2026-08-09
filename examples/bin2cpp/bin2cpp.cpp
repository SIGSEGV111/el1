#include <el1/error.hpp>
#include <el1/io_file.hpp>
#include <el1/io_stream.hpp>
#include <el1/io_text_encoding_utf8.hpp>
#include <el1/io_text_string.hpp>
#include <el1/io_text_terminal.hpp>
#include <el1/system_cmdline.hpp>

#include <memory>

int main(const int argc, char* argv[])
{
	using namespace el1;
	using namespace el1::error;
	using namespace el1::io::file;
	using namespace el1::io::stream;
	using namespace el1::io::text;
	using namespace el1::io::text::encoding::utf8;
	using namespace el1::io::text::string;
	using namespace el1::system::cmdline;

	try
	{
		TPath input_path = "-";
		TPath output_path = "-";
		TString variable_name = "data";

		ParseCmdlineArguments(argc, argv,
			THelpArgument("Convert binary data into a C++ byte array."),
			TPathArgument(&input_path, EObjectType::FILE, ECreateMode::OPEN, 'i', "input-file", "", true, false, "Binary input file; '-' reads stdin"),
			TPathArgument(&output_path, EObjectType::FILE, ECreateMode::TRUNCATE, 'o', "output-file", "", true, false, "C++ output file; '-' writes stdout"),
			TStringArgument(&variable_name, 'n', "name", "", true, false, "Name of the generated C++ variable")
		);

		std::unique_ptr<TFile> input_file;
		std::unique_ptr<TFile> output_file;

		if(input_path != TPath("-"))
		{
			input_file = New<TFile>(input_path);
		}

		if(output_path != TPath("-"))
		{
			output_file = New<TFile>(output_path, TAccess::WO, ECreateMode::TRUNCATE);
		}

		ISource<byte_t>& input = input_file ? static_cast<ISource<byte_t>&>(*input_file) : static_cast<ISource<byte_t>&>(terminal::stdin);
		ISink<byte_t>& output = output_file ? static_cast<ISink<byte_t>&>(*output_file) : static_cast<ISink<byte_t>&>(terminal::stdout);

		TNumberFormatter number_formatter(TNumberFormatter::PLAIN_OCTAL);
		number_formatter.config.integer_pad_sign = '0';
		number_formatter.config.n_min_integer_places = 3;

		TString body;
		iosize_t count = 0;
		input.Pipe().ForEach([&](const byte_t byte)
		{
			if(count > 0 && (count & 0xff) == 0)
			{
				body += L"\"\n\t\"";
			}

			count++;
			switch(byte)
			{
				case 0x00: body += L"\\0"; break;
				case 0x07: body += L"\\a"; break;
				case 0x08: body += L"\\b"; break;
				case 0x09: body += L"\\t"; break;
				case 0x0a: body += L"\\n"; break;
				case 0x0b: body += L"\\v"; break;
				case 0x0c: body += L"\\f"; break;
				case 0x0d: body += L"\\r"; break;
				case 0x22: body += L"\\\""; break;
				case 0x5c: body += L"\\\\"; break;
				default:
				{
					if(byte >= 0x20 && byte <= 0x7e)
					{
						body += static_cast<char>(byte);
					}
					else
					{
						(body += L"\\") += number_formatter.Format(byte);
					}
					break;
				}
			}
		});

		const TString header = TString::Format(
			U"const el1::io::types::byte_t %s[] =\n\t\"",
			variable_name
		);
		body += TString::Format(
			U"\";\nconst el1::io::types::usys_t %s_size = sizeof(%s) - 1;\n",
			variable_name,
			variable_name
		);

		header.chars.Pipe().Transform(TUTF8Encoder()).ToStream(output);
		body.chars.Pipe().Transform(TUTF8Encoder()).ToStream(output);
		return 0;
	}
	catch(const shutdown_t&)
	{
		return 0;
	}
	catch(const IException& exception)
	{
		exception.Print("TOP LEVEL");
		return 1;
	}
}
