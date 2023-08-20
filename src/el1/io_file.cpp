#include "io_file.hpp"

namespace el1::io::file
{
	using namespace error;

	const TAccess TAccess::NONE = { .read = false, .write = false, .execute = false };
	const TAccess TAccess::RO   = { .read = true,  .write = false, .execute = false };
	const TAccess TAccess::WO   = { .read = false, .write = true,  .execute = false };
	const TAccess TAccess::RW   = { .read = true,  .write = true,  .execute = false };
	const TAccess TAccess::RX   = { .read = true,  .write = false, .execute = true  };

	const char* TInvalidPathException::ReasonText(const EReason reason)
	{
		switch(reason)
		{
			case TInvalidPathException::EReason::EMPTY_COMPONENT:
				return "empty component";
			case TInvalidPathException::EReason::EXPECTED_RELATIVE_PATH:
				return "expected a relative path";
			case TInvalidPathException::EReason::NO_MATCH:
				return "path does not match";
			case TInvalidPathException::EReason::EMPTY_PATH:
				return "path is empty";
			case TInvalidPathException::EReason::COMPONENT_CONTAINS_SEPERATOR:
				return "path component contains the seperator character";
			case TInvalidPathException::EReason::POINTS_BEFORE_ROOT:
				return "absolute path has more back-references than leading compontents => path points before it's root";
		}

		EL_THROW(TLogicException);
	}

	TString TInvalidPathException::Message() const
	{
		return TString::Format("invalid path encountered: %q at %d", ReasonText(this->reason), this->idx_component);
	}

	IException* TInvalidPathException::Clone() const
	{
		return new TInvalidPathException(*this);
	}

	TPath& TPath::operator+=(const TPath& rhs)
	{
		EL_ERROR(rhs.IsAbsolute(), TInvalidPathException, rhs, 0, TInvalidPathException::EReason::EXPECTED_RELATIVE_PATH);
		this->components.Append(rhs.components);
		this->cached_cstr.reset();
		return *this;
	}

	TPath TPath::operator+ (const TPath& rhs) const
	{
		TPath tmp = *this;
		tmp += rhs;
		return tmp;
	}

	TPath& TPath::operator-=(const TPath& rhs)
	{
		EL_ERROR(rhs.IsAbsolute(), TInvalidPathException, rhs, 0, TInvalidPathException::EReason::EXPECTED_RELATIVE_PATH);
		EL_ERROR(rhs.components.Count() > this->components.Count(), TIndexOutOfBoundsException, 0, this->components.Count()-1, rhs.components.Count()-1);

		const usys_t offset = this->components.Count() - rhs.components.Count();
		for(usys_t i = 0; i < rhs.components.Count(); i++)
			EL_ERROR(this->components[offset + i] != rhs.components[i], TInvalidPathException, rhs, i, TInvalidPathException::EReason::NO_MATCH);

		this->components.Cut(0, rhs.components.Count());
		this->cached_cstr.reset();

		return *this;
	}

	TPath TPath::operator- (const TPath& rhs) const
	{
		TPath tmp = *this;
		tmp -= rhs;
		return tmp;
	}

	bool TPath::operator==(const TPath& rhs) const
	{
		if(this->components.Count() != rhs.components.Count())
			return false;

		for(usys_t i = 0; i < this->components.Count(); i++)
			if(this->components[i] != rhs.components[i])
				return false;

		return true;
	}

	bool TPath::operator!=(const TPath& rhs) const
	{
		return !(*this == rhs);
	}

	void TPath::Rebase(const TPath& old_prefix, const TPath& new_prefix)
	{
		EL_ERROR(this->CountCommonPrefix(old_prefix) != old_prefix.components.Count(), TInvalidArgumentException, "old_prefix", "this path doesd not start with the given prefix");
		this->components.Remove(0, old_prefix.components.Count());
		this->components.Insert(0, new_prefix.components);
		this->cached_cstr.reset();
	}

	TString TPath::FullName() const
	{
		return this->components[-1];
	}

	TString TPath::Extension() const
	{
		const usys_t pos = this->components[-1].Find(".", -1, true);
		if(pos == NEG1)
			return "";
		else
			return this->components[-1].SliceBE(pos+1, this->components[-1].Length());
	}

	TString TPath::BareName() const
	{
		const usys_t pos = this->components[-1].Find(".", -1, true);
		if(pos == NEG1)
			return this->components[-1];
		else
			return this->components[-1].SliceBE(0, pos);
	}

	TPath TPath::Parent() const
	{
		if(this->components.Count() == 0)
			return TPath::PARENT_DIR;

		if(this->components.Count() == 1 && this->IsAbsolute())
			return *this;

		TPath tmp = *this;
		tmp.Truncate(1);
		return tmp;
	}

	void TPath::ReplaceComponent(const ssys_t index, const TPath& new_path)
	{
		EL_ERROR(index != 0 && new_path.IsAbsolute(), TInvalidPathException, new_path, NEG1, TInvalidPathException::EReason::EMPTY_PATH);
		TPath tmp = *this;
		tmp.components.Remove(index, 1);
		tmp.components.Insert(index, new_path.components);
		tmp.Validate();
		tmp.Simplify();
		*this = std::move(tmp);
	}

	usys_t TPath::CountCommonPrefix(const TPath& other) const
	{
		const usys_t n_min = util::Min(this->components.Count(), other.components.Count());
		usys_t i = 0;
		for(; i < n_min && this->components[i] == other.components[i]; i++);
		return i;
	}

	usys_t TPath::StripCommonPrefix(const TPath& other)
	{
		const usys_t n_common = this->CountCommonPrefix(other);
		this->components.Remove(0, n_common);
		this->cached_cstr.reset();
		return n_common;
	}

	void TPath::MakeRelativeTo(const TPath& reference)
	{
		// ../tmp/test/xyz
		// ../test
		// => ../../../test
		// => ../tmp/test/xyz

		// ../tmp/abc
		// ../tmp/xyz
		// => ../xyz
		// => ../abc

		const usys_t n_strip = StripCommonPrefix(reference);
		const usys_t n_distinct = reference.components.Count() - n_strip;

		this->components.FillInsert(0, TPath::PARENT_DIR, n_distinct);
		this->cached_cstr.reset();
	}

	TPath::operator TString() const
	{
		if(this->components.Count() == 0)
			return TPath::CURRENT_DIR;

		TString str = this->components[0];
		for(usys_t i = 1; i < this->components.Count(); i++)
		{
			str += TPath::SEPERATOR;
			str += this->components[i];
		}
		return str;
	}

	TPath::operator const char*() const
	{
		if(EL_UNLIKELY(this->cached_cstr == nullptr))
			this->cached_cstr = this->operator TString().MakeCStr();
		return this->cached_cstr.get();
	}

	TFile TPath::CreateAsFile(const bool recursive) const
	{
		if(recursive)
			this->Parent().CreateAsDirectory(true);

		return TFile(*this, TAccess::RW, ECreateMode::EXCLUSIVE);
	}

	void TPath::Strip(const usys_t n_compontents)
	{
		this->components.Remove(0, n_compontents);
		this->cached_cstr.reset();
	}

	void TPath::Truncate(const usys_t n_compontents)
	{
		this->components.Remove(-n_compontents, n_compontents);
		this->cached_cstr.reset();
	}

	bool TPath::HasAccess(const TAccess request) const
	{
		EL_NOT_IMPLEMENTED;
	}

	void TPath::Move(const TPath& new_path, const bool overwrite) const
	{
		EL_NOT_IMPLEMENTED;
	}

	void TPath::Copy(const TPath& new_path, const bool overwrite) const
	{
		EL_NOT_IMPLEMENTED;
	}

	void TPath::Delete(const bool recursive) const
	{
		EL_NOT_IMPLEMENTED;
	}

	TPath::TPath(const TString& str) : components(str.Length() == 0 ? TList<TString>() : str.Split(TPath::SEPERATOR))
	{
		this->Validate();
	}

	TPath::TPath(const char* const str) : TPath(TString(str))
	{
	}

	TPath::TPath(const wchar_t* const str) : TPath(TString(str))
	{
	}

	TPath::TPath(std::initializer_list<TString> list) : components(list)
	{
		this->Validate();
	}

	TPath::TPath(const TPath& other) : components(other.components), cached_cstr()
	{
	}

	TPath& TPath::operator=(const TPath& rhs)
	{
		this->components = rhs.components;
		this->cached_cstr.reset();
		return *this;
	}

	/************************************************************************/

	bool TDirectory::Contains(const TString& name) const
	{
		bool result = false;
		Enum([&](const direntry_t& dent) {
			if(dent.name == name)
			{
				result = true;
				return false;
			}
			else
			{
				return true;
			}
		});
		return result;
	}

	TDirectory TDirectory::Parent() const
	{
		return TDirectory(*this, TPath::PARENT_DIR);
	}

	TDirectory::TDirectory(const TDirectory&)
	{
		EL_NOT_IMPLEMENTED;
	}

	TDirectory& TDirectory::operator=(const TDirectory&)
	{
		EL_NOT_IMPLEMENTED;
	}

	/************************************************************************/

	// void TFile::Offset(const siosize_t position, const ESeekOrigin origin)
	// {
	// 	switch(origin)
	// 	{
	// 		case ESeekOrigin::START:
	// 		{
	// 			this->offset = position;
	// 			return;
	// 		}
 //
	// 		case ESeekOrigin::END:
	// 		{
	// 			const iosize_t size = this->Size();
	// 			if(position < 0)
	// 				EL_ERROR((iosize_t)-position > size, TInvalidArgumentException, "position", "the new position would be before the start of the file (negative offset)");
	// 			this->offset = size + position;
	// 			return;
	// 		}
 //
	// 		case ESeekOrigin::CURRENT:
	// 		{
	// 			if(position < 0)
	// 				EL_ERROR((iosize_t)-position > this->offset, TInvalidArgumentException, "position", "the new position would be before the start of the file (negative offset)");
	// 			this->offset += position;
	// 			return;
	// 		}
	// 	}
 //
	// 	EL_THROW(TInvalidArgumentException, "origin", "valid values are START, END, CURRENT");
	// }
}
