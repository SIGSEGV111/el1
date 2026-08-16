#include <el1/error.hpp>
#include <el1/io_file.hpp>
#include <el1/io_format_json.hpp>
#include <el1/io_text_encoding_utf8.hpp>
#include <el1/io_text_string.hpp>
#include <el1/system_cmdline.hpp>

int main(const int argc, char* argv[])
{
	using namespace el1::error;
	using namespace el1::io::file;
	using namespace el1::io::format::json;
	using namespace el1::io::text::encoding::utf8;
	using namespace el1::system::cmdline;

	try
	{
		TPath input_path;
		TPath output_path;

		ParseCmdlineArguments(argc, argv,
			THelpArgument(U"Parse tolerant JSON and write normalized JSON."),
			TPathArgument(&input_path, EObjectType::FILE, ECreateMode::OPEN, 'i', U"input-file", U"", false, false, U"JSON input file"),
			TPathArgument(&output_path, EObjectType::FILE, ECreateMode::TRUNCATE, 'o', U"output-file", U"", false, false, U"Normalized JSON output file")
		);

		const el1::io::text::string::TString input = TFile::ReadText(input_path, false);
		TJsonValue json = TJsonValue::Parse(input, true);
		TFile output_file(output_path, TAccess::WO, ECreateMode::TRUNCATE);
		json.Pipe().Transform(TUTF8Encoder()).ToStream(output_file);
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
