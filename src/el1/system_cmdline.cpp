#include "system_cmdline.hpp"
#include "system_task.hpp"
#include "io_collection_map.hpp"
#include <iostream>

namespace el1::system::cmdline
{
	using namespace io::collection::list;
	using namespace io::collection::map;
	using namespace io::file;

	usys_t ParseCmdlineArguments(const int argc, const char* const argv[], std::initializer_list<IArgument*> defs)
	{
		EL_ERROR(argv == nullptr, TInvalidArgumentException, "argv", "argv must not be nullptr");
		EL_ERROR(argv[0] == nullptr, TInvalidArgumentException, "argv[0]", "argv[0] must not be nullptr");
		EL_ERROR(argc < 1, TInvalidArgumentException, "argc", "argc must be at least 1 (the program call name)");

		TParserState state = {
			.progname = argv[0],
			.defs = defs,
			.args = TList<TString>(),
			.shorthand_map = TSortedMap<TUTF32, IArgument*>(),
			.longname_map = TSortedMap<TString, IArgument*>(),
			.env = system::task::EnvironmentVariables(),
			.idx_argument = 0
		};

		{
			unsigned n_arg = 1;
			for(; n_arg < (unsigned)argc && argv[n_arg] != nullptr; n_arg++)
			{
				state.args.Append(argv[n_arg]);
			}
			EL_ERROR((unsigned)argc != n_arg, TLogicException);
			// EL_ERROR(argc >= 16382, TInvalidArgumentException, "argc", "maximum number of arguments is 16382");
		}

		THelpArgument arg_help;
		if(state.defs.Pipe().Filter([](const IArgument* def){ return dynamic_cast<const THelpArgument*>(def) != nullptr; }).Count() == 0)
			state.defs.Insert(0, &arg_help);

		for(IArgument* def : state.defs)
		{
			if(def->name.Length() > 0)
				EL_ANNOTATE_ERROR(state.longname_map.Add(def->name, def), TException, TString::Format("while processing option name %q", def->name));

			if(def->shorthand != TUTF32::TERMINATOR)
				EL_ANNOTATE_ERROR(state.shorthand_map.Add(def->shorthand, def), TException, TString::Format("while processing option with shorthand %q", def->shorthand));
		}

		return IArgument::ParseCmdlineArguments(state);
	}

	IArgument::IArgument(const TUTF32 shorthand, TString name, TString env, const bool optional, const bool anonymous, TString help, const EArgumentType type) : name(std::move(name)), env(std::move(env)), help(std::move(help)), shorthand(shorthand), optional(optional ? 1 : 0), anonymous(anonymous ? 1 : 0), type((u8_t)type), assigned(0)
	{
		EL_ERROR(name.chars.Contains('='), TInvalidArgumentException, "name", "the argument name must not contain '=' signs");
		EL_ERROR( env.chars.Contains('='), TInvalidArgumentException, "env", "the environment variable name must not contain '=' signs");
		EL_ERROR(shorthand == '-', TInvalidArgumentException, "shorthand", "the shorthand cannot be the '-' sign, since \"--\" is used as argument list terminator");
	}

	static bool ParseBool(TString& argv)
	{
		argv.ToLower();

		if(argv == "false" || argv == "no" || argv == "disabled" || argv == "0")
			return false;

		if(argv == "true" || argv == "yes" || argv == "enabled" || argv == "1")
			return true;

		EL_THROW(TInvalidArgumentException, "argv", "only true/false, yes/no, enabled/disabled, 1/0 are supported as values for flags and boolean arguments");
	}

	/************************************/

	void TFlagArgument::ParseValue(TString& argv, const TParserState& state)
	{
		*var = ParseBool(argv);
	}

	TString TFlagArgument::DefaultValue() const
	{
		return "false";
	}

	TString TFlagArgument::ExpectedType() const
	{
		return "boolean";
	}

	TFlagArgument::TFlagArgument(bool* const var, const TUTF32 shorthand, TString name, TString env, TString help) : IArgument(shorthand, std::move(name), std::move(env), true, false, std::move(help), EArgumentType::FLAG), var(var)
	{
		*var = false;
	}

	/************************************/

	void TBooleanArgument::ParseValue(TString& argv, const TParserState& state)
	{
		*var = ParseBool(argv);
	}

	TString TBooleanArgument::DefaultValue() const
	{
		return *var ? "true" : "false";
	}

	TString TBooleanArgument::ExpectedType() const
	{
		return "boolean";
	}

	TBooleanArgument::TBooleanArgument(bool* const var, const TUTF32 shorthand, TString name, TString env, const bool optional, const bool anonymous, TString help) : IArgument(shorthand, std::move(name), std::move(env), optional, anonymous, std::move(help), EArgumentType::KEY_VALUE), var(var)
	{
	}

	/************************************/

	void TStringArgument::ParseValue(TString& argv, const TParserState& state)
	{
		*var = argv;
	}

	TString TStringArgument::DefaultValue() const
	{
		return *var;
	}

	TString TStringArgument::ExpectedType() const
	{
		return "string";
	}

	TStringArgument::TStringArgument(TString* const var, const TUTF32 shorthand, TString name, TString env, const bool optional, const bool anonymous, TString help) : IArgument(shorthand, std::move(name), std::move(env), optional, anonymous, std::move(help), EArgumentType::KEY_VALUE), var(var)
	{
	}

	/************************************/

	void TIntegerArgument::ParseValue(TString& argv, const TParserState& state)
	{
		*var = argv.ToInteger();
	}

	TString TIntegerArgument::DefaultValue() const
	{
		return TString::Format("%d", *var);
	}

	TString TIntegerArgument::ExpectedType() const
	{
		return "integer";
	}

	TIntegerArgument::TIntegerArgument(s64_t* const var, const TUTF32 shorthand, TString name, TString env, const bool optional, const bool anonymous, TString help) : IArgument(shorthand, std::move(name), std::move(env), optional, anonymous, std::move(help), EArgumentType::KEY_VALUE), var(var)
	{
	}

	/************************************/

	void TFloatArgument::ParseValue(TString& argv, const TParserState& state)
	{
		*var = argv.ToDouble();
	}

	TString TFloatArgument::DefaultValue() const
	{
		return TString::Format("%d", *var);
	}

	TString TFloatArgument::ExpectedType() const
	{
		return "decimal";
	}

	TFloatArgument::TFloatArgument(double* const var, const TUTF32 shorthand, TString name, TString env, const bool optional, const bool anonymous, TString help) : IArgument(shorthand, std::move(name), std::move(env), optional, anonymous, std::move(help), EArgumentType::KEY_VALUE), var(var)
	{
	}

	/************************************/

	void TArrayArgument::ParseValue(TString& argv, const TParserState& state)
	{
		*var = argv.Split(delimiter);
	}

	TString TArrayArgument::DefaultValue() const
	{
		TString str;
		var->Pipe().ForEach([&str](const TString& item) { str += item; str += ','; });
		str.Cut(0,1);
		return str;
	}

	TString TArrayArgument::ExpectedType() const
	{
		return "array";
	}

	TArrayArgument::TArrayArgument(TList<TString>* const var, const TString delimiter, const TUTF32 shorthand, TString name, TString env, const bool optional, const bool anonymous, TString help) : IArgument(shorthand, std::move(name), std::move(env), optional, anonymous, std::move(help), EArgumentType::ARRAY), var(var), delimiter(delimiter)
	{
	}

	/************************************/

	static const char* ObjectTypeToString(const EObjectType type)
	{
		switch(type)
		{
			case EObjectType::NX:
			case EObjectType::UNKNOWN:
				return nullptr;

			case EObjectType::FILE:
				return "regular file";

			case EObjectType::DIRECTORY:
				return "directory";

			case EObjectType::CHAR_DEVICE:
				return "character device";

			case EObjectType::BLOCK_DEVICE:
				return "socket";

			case EObjectType::FIFO:
				return "fifo";

			case EObjectType::SYMLINK:
				return "symbolic link";

			case EObjectType::SOCKET:
				return "socket";
		}

		EL_THROW(TLogicException);
	}

	void TPathArgument::ParseValue(TString& argv, const TParserState& state)
	{
		*var = argv;
		var->MakeAbsolute();
		var->Simplify();

		EL_ERROR(create_mode == ECreateMode::OPEN && var->Type() == EObjectType::NX, TException, TString::Format("no such file or directory %q", var->operator TString()));

		EL_ERROR(create_mode == ECreateMode::EXCLUSIVE && var->Type() != EObjectType::NX, TException, TString::Format("expected to create %q, but it already exists", var->operator TString()));

		EL_ERROR(var->Type() != EObjectType::NX && var->Type() != expected_type, TException, TString::Format("expected %q to be a %s, but it is actually a %s", var->operator TString(), ObjectTypeToString(expected_type), ObjectTypeToString(var->Type())));
	}

	TString TPathArgument::DefaultValue() const
	{
		return var->IsEmpty() ? TString() : TString(*var);
	}

	TString TPathArgument::ExpectedType() const
	{
		TString str_create_mode;
		TString str_expected_type;

		switch(create_mode)
		{
			case ECreateMode::OPEN:
				str_create_mode = "existing";
				break;

			case ECreateMode::NX:
			case ECreateMode::TRUNCATE:
			case ECreateMode::DELETE:
				str_create_mode = "output";
				break;

			case ECreateMode::EXCLUSIVE:
				str_create_mode = "not-existing";
				break;

			default:
				EL_THROW(TLogicException);
		}

		switch(expected_type)
		{
			case EObjectType::UNKNOWN:
				str_expected_type = "path";
				break;

			case EObjectType::FILE:
				str_expected_type = "file";
				break;

			case EObjectType::DIRECTORY:
				str_expected_type = "directory";
				break;

			case EObjectType::CHAR_DEVICE:
				str_expected_type = "chardev";
				break;

			case EObjectType::BLOCK_DEVICE:
				str_expected_type = "blockdev";
				break;

			case EObjectType::FIFO:
				str_expected_type = "fifo";
				break;

			case EObjectType::SYMLINK:
				str_expected_type = "symlink";
				break;

			case EObjectType::SOCKET:
				str_expected_type = "socket";
				break;

			case EObjectType::NX:
			default:
				EL_THROW(TLogicException);
		}

		return str_create_mode + " " + str_expected_type;
	}

	TPathArgument::TPathArgument(TPath* const var, const TUTF32 shorthand, TString name, TString env, const bool optional, const bool anonymous, TString help) : IArgument(shorthand, std::move(name), std::move(env), optional, anonymous, std::move(help), EArgumentType::KEY_VALUE), var(var), expected_type(EObjectType::UNKNOWN), create_mode(ECreateMode::OPEN)
	{
	}

	TPathArgument::TPathArgument(TPath* const var, const EObjectType expected_type, const io::file::ECreateMode create_mode, const TUTF32 shorthand, TString name, TString env, const bool optional, const bool anonymous, TString help) : IArgument(shorthand, std::move(name), std::move(env), optional, anonymous, std::move(help), EArgumentType::KEY_VALUE), var(var), expected_type(expected_type), create_mode(create_mode)
	{
		EL_ERROR(expected_type == EObjectType::NX, TInvalidArgumentException, "expected_type", "expected type cannot be NX");
	}

	/************************************/

	void THelpArgument::ParseValue(TString& argv, const TParserState& state)
	{
		if(ParseBool(argv))
		{
			ShowHelpGenerated(state.progname, state.defs);
			throw shutdown_t();
		}
	}

	TString THelpArgument::DefaultValue() const
	{
		return "false";
	}

	TString THelpArgument::ExpectedType() const
	{
		return "boolean";
	}

	THelpArgument::THelpArgument(TString program_description, TString website_url, TString bugtracker_url, TString scm_url, const TUTF32 shorthand) : IArgument(shorthand, "help", "", true, false, "shows the command-line help text", EArgumentType::FLAG)
	{
	}

	/************************************/

	void IArgument::ShowHelpGenerated(const TString& progname, const TList<IArgument*>& defs)
	{
		TString usage = TString::Format("usage: %q", progname);

		const bool multi_line = defs.Count() > 4 || defs.Pipe().Filter([](auto def) { return def->help.Length() > 0; }).Count() > 0;
		if(multi_line)
			usage += " ...";

		TList<TString> lines;

		for(const IArgument* def : defs)
			if(def->name.Length() > 0 || def->shorthand != TUTF32::TERMINATOR)
			{
				TString line;
				line += def->optional ? '[' : '{';

				if(def->shorthand != TUTF32::TERMINATOR)
				{
					line += "-";
					line += def->shorthand;
				}

				if(def->name.Length() > 0 && def->shorthand != TUTF32::TERMINATOR)
					line += '|';

				if(def->name.Length() > 0)
				{
					line += "--";
					line += def->name;
				}

				if(def->type != (u8_t)EArgumentType::FLAG)
				{
					TString default_value;
					if(def->optional && (default_value = def->DefaultValue()).Length() > 0)
					{
						line += '=';
						line += default_value;
					}
					else
					{
						line += "=<";
						line += def->ExpectedType();
						line += '>';
					}
				}

				line += def->optional ? ']' : '}';

				if(multi_line)
					lines.MoveAppend(std::move(line));
				else
				{
					usage += ' ';
					usage += line;
				}
			}

		if(multi_line)
		{
			const usys_t n_max_len = lines.Pipe().Aggregate([](usys_t& max, const TString& line){ max = el1::util::Max(max, line.Length()); }, (usys_t)0);

			for(usys_t i = 0; i < lines.Count(); i++)
			{
				const TString& line = lines[i];
				const IArgument* def = defs[i];

				usage += "\n    ";
				usage += line;

				if(def->help.Length() > 0)
				{
					const unsigned n_align_add = n_max_len - line.Length();
					usage += ' ';
					for(usys_t i = 0; i < n_align_add; i++)
						usage += ' ';
					usage += def->help;
				}
			}
		}

		usage += '\n';
		std::cerr<<usage.MakeCStr().get();
	}

	usys_t IArgument::ParseCmdlineArguments(const TParserState& state)
	{
		usys_t idx_anonymous_scan = 0;
		bool eof = false;

		for(usys_t i = 0; i < state.args.Count(); i++)
		{
			const TString& arg = state.args[i];
			state.idx_argument = i;

			IArgument* def = nullptr;
			kv_pair_tt<TString,TString> kv;

			if(!eof && arg[0] == '-' && arg.Length() > 1)
			{
				if(arg[1] == '-')
				{
					if(arg.Length() == 2) // == "--" => EOF
					{
						// all folowing arguments are anonymous
						eof = true;
						continue;
					}
					else
					{
						// longname
						if(arg.chars.Contains('='))
							// key=value style
							kv = arg.SplitKV('=');
						else
							// key only, next argument has the value (if the argument needs a value)
							kv.key = arg;

						kv.key.Cut(2,0);
						EL_ANNOTATE_ERROR(def = state.longname_map[kv.key], TException, TString::Format("unrecognized option %q @ pos %d", kv.key, state.idx_argument + 1));

						if(!arg.chars.Contains('='))
						{
							if(def->type == (u8_t)EArgumentType::FLAG)
							{
								kv.value = L"true";
							}
							else
							{
								i++;
								EL_ANNOTATE_ERROR(kv.value = state.args[i], TException, TString::Format("missing value for option %q @ pos %d", kv.key, state.idx_argument + 1));
							}
						}
					}
				}
				else
				{
					// shorthand
					// shorthand arguments never use the key=value syntax
					// thus the value (if any) is always in the next argument
					EL_ERROR(arg.Length() > 2, TException, TString::Format("unrecognized option %q @ pos %d", arg, state.idx_argument + 1));
					EL_ANNOTATE_ERROR(def = state.shorthand_map[arg[1]], TException, TString::Format("unrecognized option %q @ pos %d", arg, state.idx_argument + 1));

					kv.key = arg.SliceSL(1,1);

					if(def->type == (u8_t)EArgumentType::FLAG)
					{
						kv.value = L"true";
					}
					else
					{
						i++;
						EL_ANNOTATE_ERROR(kv.value = state.args[i], TException, TString::Format("missing value for option %q @ pos %d", kv.key, state.idx_argument + 1));
					}
				}
			}
			else
			{
				// anonymous argument amongst named arguments (or past EOF)
				kv.key = L"<anonymous option>";
				kv.value = arg;

				for(; idx_anonymous_scan < state.defs.Count(); idx_anonymous_scan++)
					if(state.defs[idx_anonymous_scan]->anonymous == 1 && state.defs[idx_anonymous_scan]->assigned == 0)
					{
						def = state.defs[idx_anonymous_scan];
						idx_anonymous_scan++;
						break;
					}

				if(def == nullptr && eof)
					return i;

				EL_ERROR(def == nullptr, TException, TString::Format("unable to match anonymous value %q with any defined option", arg));
			}

			EL_ERROR(def == nullptr, TLogicException);
			EL_ERROR(def->assigned == 1, TException, TString::Format("option value for %q specified multiple times", kv.key));
			EL_ANNOTATE_ERROR(def->ParseValue(kv.value, state), TException, TString::Format("unable to parse value %q for option %q at pos %d", kv.value, kv.key, state.idx_argument + 1));
			def->assigned = 1;
		}

		for(const IArgument* def : state.defs)
			EL_ERROR(def->assigned == 0 && def->optional == 0, TException, TString::Format("option %q is not optional but was not specified", def->name.Length() > 0 ? def->name : TString() += def->shorthand));

		return state.args.Count();
	}
}
