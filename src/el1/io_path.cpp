#include "io_path.hpp"

namespace el1::io::path
{
	using namespace error;

	const char* TInvalidPathException::ReasonText(const EReason reason)
	{
		switch(reason)
		{
			case EReason::EMPTY_COMPONENT:
				return "empty component";
			case EReason::EXPECTED_RELATIVE_PATH:
				return "expected a relative path";
			case EReason::NO_MATCH:
				return "path does not match";
			case EReason::EMPTY_PATH:
				return "path is empty";
			case EReason::COMPONENT_CONTAINS_SEPERATOR:
				return "path component contains the seperator character";
			case EReason::POINTS_BEFORE_ROOT:
				return "absolute path has more back-references than leading compontents => path points before its root";
		}

		EL_THROW(TLogicException);
	}

	TString TInvalidPathException::Message() const
	{
		return TString::Format("invalid path encountered: %q at %d", ReasonText(reason), idx_component);
	}

	IException* TInvalidPathException::Clone() const
	{
		return new TInvalidPathException(*this);
	}

	void TPath::Validate()
	{
		for(usys_t i = 0; i < components.Count(); i++)
		{
			EL_ERROR(components[i].Find(separator) != NEG1, TInvalidPathException, *this, i, TInvalidPathException::EReason::COMPONENT_CONTAINS_SEPERATOR);
			EL_ERROR(components[i].Length() == 0 && !(allow_absolute && i == 0), TInvalidPathException, *this, i, TInvalidPathException::EReason::EMPTY_COMPONENT);
		}
	}

	void TPath::ValidateCompatibility(const TPath& other) const
	{
		EL_ERROR(separator != other.separator || allow_absolute != other.allow_absolute, TInvalidArgumentException, "other", "path syntax does not match");
	}

	TPath& TPath::operator+=(const TPath& rhs)
	{
		ValidateCompatibility(rhs);
		EL_ERROR(rhs.IsAbsolute(), TInvalidPathException, rhs, 0, TInvalidPathException::EReason::EXPECTED_RELATIVE_PATH);
		components.Append(rhs.components);
		cached_cstr.reset();
		return *this;
	}

	TPath TPath::operator+(const TPath& rhs) const
	{
		TPath tmp = *this;
		tmp += rhs;
		return tmp;
	}

	TPath& TPath::operator-=(const TPath& rhs)
	{
		ValidateCompatibility(rhs);
		EL_ERROR(rhs.IsAbsolute(), TInvalidPathException, rhs, 0, TInvalidPathException::EReason::EXPECTED_RELATIVE_PATH);
		EL_ERROR(rhs.components.Count() > components.Count(), TIndexOutOfBoundsException, 0, components.Count() - 1, rhs.components.Count() - 1);

		const usys_t offset = components.Count() - rhs.components.Count();
		for(usys_t i = 0; i < rhs.components.Count(); i++)
			EL_ERROR(components[offset + i] != rhs.components[i], TInvalidPathException, rhs, i, TInvalidPathException::EReason::NO_MATCH);

		components.Cut(0, rhs.components.Count());
		cached_cstr.reset();
		return *this;
	}

	TPath TPath::operator-(const TPath& rhs) const
	{
		TPath tmp = *this;
		tmp -= rhs;
		return tmp;
	}

	bool TPath::operator==(const TPath& rhs) const
	{
		if(separator != rhs.separator || allow_absolute != rhs.allow_absolute || components.Count() != rhs.components.Count())
			return false;

		for(usys_t i = 0; i < components.Count(); i++)
			if(components[i] != rhs.components[i])
				return false;

		return true;
	}

	bool TPath::operator!=(const TPath& rhs) const
	{
		return !(*this == rhs);
	}

	void TPath::Rebase(const TPath& old_prefix, const TPath& new_prefix)
	{
		ValidateCompatibility(old_prefix);
		ValidateCompatibility(new_prefix);
		EL_ERROR(CountCommonPrefix(old_prefix) != old_prefix.components.Count(), TInvalidArgumentException, "old_prefix", "this path does not start with the given prefix");
		components.Remove(0, old_prefix.components.Count());
		components.Insert(0, new_prefix.components);
		cached_cstr.reset();
	}

	TPath TPath::Parent() const
	{
		if(components.Count() == 0 || (components.Count() == 1 && IsAbsolute()))
			return *this;

		TPath tmp = *this;
		tmp.Truncate(1);
		return tmp;
	}

	void TPath::ReplaceComponent(const ssys_t index, const TPath& new_path)
	{
		ValidateCompatibility(new_path);
		EL_ERROR(index != 0 && new_path.IsAbsolute(), TInvalidPathException, new_path, NEG1, TInvalidPathException::EReason::EXPECTED_RELATIVE_PATH);
		TPath tmp = *this;
		tmp.components.Remove(index, 1);
		tmp.components.Insert(index, new_path.components);
		tmp.Validate();
		*this = std::move(tmp);
	}

	usys_t TPath::CountCommonPrefix(const TPath& other) const
	{
		ValidateCompatibility(other);
		const usys_t n_min = util::Min(components.Count(), other.components.Count());
		usys_t i = 0;
		for(; i < n_min && components[i] == other.components[i]; i++);
		return i;
	}

	usys_t TPath::StripCommonPrefix(const TPath& other)
	{
		const usys_t n_common = CountCommonPrefix(other);
		components.Remove(0, n_common);
		cached_cstr.reset();
		return n_common;
	}

	void TPath::Strip(const usys_t n_components)
	{
		components.Remove(0, n_components);
		cached_cstr.reset();
	}

	void TPath::Truncate(const usys_t n_components)
	{
		components.Remove(-n_components, n_components);
		cached_cstr.reset();
	}

	bool TPath::IsAbsolute() const
	{
		return allow_absolute && components.Count() > 0 && components[0].Length() == 0;
	}

	TPath::operator TString() const
	{
		if(components.Count() == 0)
			return "";

		TString str = components[0];
		for(usys_t i = 1; i < components.Count(); i++)
		{
			str += separator;
			str += components[i];
		}
		return str;
	}

	TPath::operator const char*() const
	{
		if(EL_UNLIKELY(cached_cstr == nullptr))
			cached_cstr = this->operator TString().MakeCStr();
		return cached_cstr.get();
	}

	TPath::TPath(const TUTF32 separator, const bool allow_absolute) : separator(separator), allow_absolute(allow_absolute)
	{
	}

	TPath::TPath(const TString& str, const TUTF32 separator, const bool allow_absolute) :
		components(str.Length() == 0 ? TList<TString>() : str.Split(separator)),
		separator(separator),
		allow_absolute(allow_absolute)
	{
		if(allow_absolute && str.Length() == 1 && str[0] == separator && components.Count() == 2)
			components.Remove(-1, 1);
		Validate();
	}

	TPath::TPath(std::initializer_list<TString> list, const TUTF32 separator, const bool allow_absolute) : components(list), separator(separator), allow_absolute(allow_absolute)
	{
		Validate();
	}

	TPath::TPath(const TUTF32 separator) : TPath(separator, false)
	{
	}

	TPath::TPath(const TString& str, const TUTF32 separator) : TPath(str, separator, false)
	{
	}

	TPath::TPath(const char* const str, const TUTF32 separator) : TPath(TString(str), separator)
	{
	}

	TPath::TPath(const wchar_t* const str, const TUTF32 separator) : TPath(TString(str), separator)
	{
	}

	TPath::TPath(std::initializer_list<TString> list, const TUTF32 separator) : TPath(list, separator, false)
	{
	}

	TPath::TPath(const TPath& other) : components(other.components), separator(other.separator), allow_absolute(other.allow_absolute)
	{
	}

	TPath& TPath::operator=(const TPath& rhs)
	{
		components = rhs.components;
		separator = rhs.separator;
		allow_absolute = rhs.allow_absolute;
		cached_cstr.reset();
		return *this;
	}
}
