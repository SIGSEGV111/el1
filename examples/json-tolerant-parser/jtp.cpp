#include <el1/el1.cpp>

int main(int argc, char* argv[])
{
	using namespace el1::error;
	using namespace el1::system::cmdline;
	using namespace el1::io::types;
	using namespace el1::io::file;
	using namespace el1::io::text;
	using namespace el1::io::text::string;
	using namespace el1::io::text::encoding::utf8;
	using namespace el1::io::format::json;

	try
	{
		TPath in;
		TPath out;

		ParseCmdlineArguments(argc, argv,
			TPathArgument(&in, EObjectType::FILE, ECreateMode::OPEN, 'i', "input-file", "", false, false, "JSON input file"),
			TPathArgument(&out, EObjectType::FILE, ECreateMode::TRUNCATE, 'o', "output-file", "", false, false, "JSON output file")
		);

		TFile in_file(in);
		TMapping map(&in_file, TAccess::RO);
		TFile out_file(out, TAccess::WO, ECreateMode::TRUNCATE);
		map.Pipe().Transform(TUTF8Decoder()).Transform(TJsonParser(true)).ForEach([&out_file](auto& j) { j.Pipe().Transform(TUTF8Encoder()).ToStream(out_file); });
	}
	catch(shutdown_t)
	{
	}
	catch(const IException& e)
	{
		e.Print("TOP LEVEL");
		return 1;
	}

	return 0;
}
