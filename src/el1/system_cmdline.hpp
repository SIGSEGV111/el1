#pragma once

#include "io_text_string.hpp"
#include "io_collection_list.hpp"
#include "io_collection_map.hpp"
#include "io_file.hpp"

namespace el1::system::cmdline
{
	using namespace io::text::string;

	class IArgument;

	enum class EArgumentType : u8_t
	{
		FLAG,
		KEY_VALUE,
		ARRAY
	};

	struct TParserState
	{
		TString progname;
		io::collection::list::TList<IArgument*> defs;
		io::collection::list::TList<TString> args;
		io::collection::map::TSortedMap<TUTF32, IArgument*> shorthand_map;
		io::collection::map::TSortedMap<TString, IArgument*> longname_map;
		const io::collection::map::TSortedMap<TString, const TString>& env;

		mutable usys_t idx_argument;
	};

	class IArgument
	{
		protected:
			virtual void ParseValue(const TString& value, const TParserState& state) = 0;
			virtual TString DefaultValue() const EL_GETTER = 0;
			virtual TString ExpectedType() const EL_GETTER = 0;

			IArgument(const TUTF32 shorthand, TString name, TString env, const bool optional, const bool anonymous, TString help, const EArgumentType type);

		public:
			static usys_t ParseCmdlineArguments(const TParserState& state);
			static void ShowHelpGenerated(const TString& progname, const TList<IArgument*>& defs);

			const TString name;
			const TString env;
			const TString help;
			const TUTF32 shorthand;
			const u32_t optional : 1;
			const u32_t anonymous : 1;
			const u32_t type : 2;
			u32_t assigned : 1;
	};

	class TShowVersionArgument : public IArgument
	{
		protected:
			const char* const proginfo;

			void ParseValue(const TString& value, const TParserState& state) final override;
			TString DefaultValue() const final override EL_GETTER;
			TString ExpectedType() const final override EL_GETTER;

		public:
			TShowVersionArgument(const TUTF32 shorthand, TString name, const char* const proginfo);
			TShowVersionArgument(const char* const proginfo);
	};

	class TFlagArgument : public IArgument
	{
		protected:
			bool* const var;

			void ParseValue(const TString& value, const TParserState& state) final override;
			TString DefaultValue() const final override EL_GETTER;
			TString ExpectedType() const final override EL_GETTER;

		public:
			bool Value() const EL_GETTER { return *var; }
			TFlagArgument(bool* const var, const TUTF32 shorthand, TString name, TString env = L"", TString help = L"");
	};

	class TBooleanArgument : public IArgument
	{
		protected:
			bool* const var;

			void ParseValue(const TString& value, const TParserState& state) final override;
			TString DefaultValue() const final override EL_GETTER;
			TString ExpectedType() const final override EL_GETTER;

		public:
			bool Value() const EL_GETTER { return *var; }
			TBooleanArgument(bool* const var, const TUTF32 shorthand, TString name, TString env = L"", const bool optional = false, const bool anonymous = false, TString help = L"");
	};


	class TStringArgument : public IArgument
	{
		protected:
			TString* const var;

			void ParseValue(const TString& value, const TParserState& state) final override;
			TString DefaultValue() const final override EL_GETTER;
			TString ExpectedType() const final override EL_GETTER;

		public:
			const TString& Value() const EL_GETTER { return *var; }
			TStringArgument(TString* const var, const TUTF32 shorthand, TString name, TString env = L"", const bool optional = false, const bool anonymous = false, TString help = L"");
	};

	class TIntegerArgument : public IArgument
	{
		protected:
			s64_t* const var;

			void ParseValue(const TString& value, const TParserState& state) final override;
			TString DefaultValue() const final override EL_GETTER;
			TString ExpectedType() const final override EL_GETTER;

		public:
			s64_t Value() const EL_GETTER { return *var; }
			TIntegerArgument(s64_t* const var, const TUTF32 shorthand, TString name, TString env = L"", const bool optional = false, const bool anonymous = false, TString help = L"");
	};

	class TFloatArgument : public IArgument
	{
		protected:
			double* const var;

			void ParseValue(const TString& value, const TParserState& state) final override;
			TString DefaultValue() const final override EL_GETTER;
			TString ExpectedType() const final override EL_GETTER;

		public:
			double Value() const EL_GETTER { return *var; }
			TFloatArgument(double* const var, const TUTF32 shorthand, TString name, TString env = L"", const bool optional = false, const bool anonymous = false, TString help = L"");
	};

	class TArrayArgument : public IArgument
	{
		protected:
			io::collection::list::TList<TString>* const var;
			const TString delimiter;

			void ParseValue(const TString& value, const TParserState& state) final override;
			TString DefaultValue() const final override EL_GETTER;
			TString ExpectedType() const final override EL_GETTER;

		public:
			const io::collection::list::TList<TString>& Value() const EL_GETTER { return *var; }
			TArrayArgument(io::collection::list::TList<TString>* const var, const TString delimiter, const TUTF32 shorthand, TString name, TString env = L"", const bool optional = false, const bool anonymous = false, TString help = L"");
	};

	class TPathArgument : public IArgument
	{
		protected:
			io::file::TPath* const var;
			const io::file::EObjectType expected_type;
			const io::file::ECreateMode create_mode;

			void ParseValue(const TString& value, const TParserState& state) final override;
			TString DefaultValue() const final override EL_GETTER;
			TString ExpectedType() const final override EL_GETTER;

		public:
			const io::file::TPath& Value() const EL_GETTER { return *var; }

			TPathArgument(io::file::TPath* const var, const TUTF32 shorthand, TString name, TString env = L"", const bool optional = false, const bool anonymous = false, TString help = L"");

			TPathArgument(io::file::TPath* const var, const io::file::EObjectType expected_type, const io::file::ECreateMode create_mode, const TUTF32 shorthand, TString name, TString env = L"", const bool optional = false, const bool anonymous = false, TString help = L"");
	};

	class THelpArgument : public IArgument
	{
		protected:
			const TString program_description;
			const TString website_url;
			const TString bugtracker_url;
			const TString scm_url;

			void ParseValue(const TString& value, const TParserState& state) final override;
			TString DefaultValue() const final override EL_GETTER;
			TString ExpectedType() const final override EL_GETTER;

		public:
			THelpArgument(TString program_description = L"", TString website_url = L"", TString bugtracker_url = L"", TString scm_url = L"", const TUTF32 shorthand = 'h');
	};

	// class TVersionArgument : public IArgument
	// {
	// };

	// class TEnumArgument : public IArgument
	// {
	// 	protected:
	// 		const io::collection::list::TList<TString> allowed_values;
	// 		usys_t* const idx_value;
	// 		TString* const var;
	// };

	// Returns the number of arguments processed. This will be the same as argc - unless a "--" was encountered
	usys_t ParseCmdlineArguments(const int argc, const char* const argv[], std::initializer_list<IArgument*> defs);

	template<typename ... A>
	usys_t ParseCmdlineArguments(const int argc, const char* const argv[], const A& ... defs)
	{
		return ParseCmdlineArguments(argc, argv, { const_cast<IArgument*>(static_cast<const IArgument*>(&defs)) ... });
	}

	// void test_main(int argc, char* argv[])
	// {
	// 	TString db_user = "postgres";
	// 	TString directory;
 //
	// 	ParseCmdlineArguments(argc, argv,
	// 		THelpArgument("demo program v1"),
	// 		TStringArgument(&db_user, 'u', "database-user", "USER", false, false, "the username for DB auth"),
	// 		TStringArgument(&directory, 'd', "directory", "", false, false, "some text")
	// 	);
	// }
}
