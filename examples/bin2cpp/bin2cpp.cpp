#include <el1/el1.cpp>

int main(int argc, char* argv[])
{
	using namespace el1::error;
	using namespace el1::io::types;
	using namespace el1::io::file;
	using namespace el1::system::cmdline;
	using namespace el1::io::text;
	using namespace el1::io::text::string;
	using namespace el1::io::text::encoding::utf8;

	try
	{
		TPath in = "-";
		TPath out = "-";
		TString name = "data";

		ParseCmdlineArguments(argc, argv,
			TPathArgument(&in, EObjectType::FILE, ECreateMode::OPEN, 'i', "input-file", "", true, false, "binary input file to be converted"),
			TPathArgument(&out, EObjectType::FILE, ECreateMode::TRUNCATE, 'o', "output-file", "", true, false, "C++ output file"),
			TStringArgument(&name, 'n', "name", "", true, false, "the name of the C++ variable to hold the data")
		);

		std::unique_ptr<TFile> infile  = in  == TPath("-") ? nullptr : std::unique_ptr<TFile>(new TFile(in));
		std::unique_ptr<TFile> outfile = out == TPath("-") ? nullptr : std::unique_ptr<TFile>(new TFile(out, TAccess::WO, ECreateMode::TRUNCATE));

		ISource<byte_t>& is = in == TPath("-") ? (ISource<byte_t>&)terminal::stdin : (ISource<byte_t>&)*infile;
		ISink<byte_t>& os = out == TPath("-") ? (ISink<byte_t>&)terminal::stdout : (ISink<byte_t>&)*outfile;

		TNumberFormatter nf(TNumberFormatter::PLAIN_OCTAL);
		nf.config.integer_pad_sign = '0';
		nf.config.n_min_integer_places = 3;

		TString s;
		iosize_t n = 0;
		is.Pipe().ForEach([&](const byte_t& b){
			n++;
			if((n & 0xff) == 0)
				s += L"\"\n\t\"";

			if(b == 0)
				s += L"\\0";
			else if(b == 0x07)
				s += L"\\a";
			else if(b == 0x08)
				s += L"\\b";
			else if(b == 0x09)
				s += L"\\t";
			else if(b == 0x0a)
				s += L"\\n";
			else if(b == 0x0b)
				s += L"\\v";
			else if(b == 0x0c)
				s += L"\\f";
			else if(b == 0x0d)
				s += L"\\r";
			else if(b == 0x22)
				s += L"\\\"";
			else if(b == 0x5C)
				s += L"\\\\";
			else if(b >= 0x20 && b <= 0x7f)
				s += (char)b;
			else
				(s += L"\\") += nf.Format(b);
		});
		s += L"\";";

		TString h = TString::Format(L"const el1::io::types::byte_t %s[%d] = \n\t\"", name, n);
		h.chars.Pipe().Transform(TUTF8Encoder()).ToStream(os);
		s.chars.Pipe().Transform(TUTF8Encoder()).ToStream(os);
		return 0;
	}
	catch(shutdown_t)
	{
		return 0;
	}
	catch(const IException& e)
	{
		e.Print("TOP LEVEL");
		return 1;
	}

	return 2;
}
