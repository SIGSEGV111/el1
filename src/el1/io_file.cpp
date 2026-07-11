#include "io_file.hpp"

namespace el1::io::file
{
	using namespace error;

	const TAccess TAccess::NONE = { .read = false, .write = false, .execute = false };
	const TAccess TAccess::RO   = { .read = true,  .write = false, .execute = false };
	const TAccess TAccess::WO   = { .read = false, .write = true,  .execute = false };
	const TAccess TAccess::RW   = { .read = true,  .write = true,  .execute = false };
	const TAccess TAccess::RX   = { .read = true,  .write = false, .execute = true  };

	TPath& TPath::operator+=(const TPath& rhs)
	{
		TBase::operator+=(rhs);
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
		TBase::operator-=(rhs);
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
		return TBase::operator==(rhs);
	}

	bool TPath::operator!=(const TPath& rhs) const
	{
		return TBase::operator!=(rhs);
	}

	void TPath::Rebase(const TPath& old_prefix, const TPath& new_prefix)
	{
		TBase::Rebase(old_prefix, new_prefix);
	}

	usys_t TPath::CountCommonPrefix(const TPath& other) const
	{
		return TBase::CountCommonPrefix(other);
	}

	usys_t TPath::StripCommonPrefix(const TPath& other)
	{
		return TBase::StripCommonPrefix(other);
	}

	TString TPath::FullName() const
	{
		return components[-1];
	}

	TString TPath::Extension() const
	{
		const usys_t pos = components[-1].Find(".", -1, true);
		if(pos == NEG1)
			return "";
		else
			return components[-1].SliceBE(pos + 1, components[-1].Length());
	}

	TString TPath::BareName() const
	{
		const usys_t pos = components[-1].Find(".", -1, true);
		if(pos == NEG1)
			return components[-1];
		else
			return components[-1].SliceBE(0, pos);
	}

	TPath TPath::Parent() const
	{
		if(components.Count() == 0)
			return TPath::PARENT_DIR;

		if(components.Count() == 1 && IsAbsolute())
			return *this;

		TPath tmp = *this;
		tmp.Truncate(1);
		return tmp;
	}

	void TPath::ReplaceComponent(const ssys_t index, const TPath& new_path)
	{
		EL_ERROR(index != 0 && new_path.IsAbsolute(), TInvalidPathException, new_path, NEG1, TInvalidPathException::EReason::EXPECTED_RELATIVE_PATH);
		TPath tmp = *this;
		tmp.components.Remove(index, 1);
		tmp.components.Insert(index, new_path.components);
		tmp.Validate();
		tmp.Simplify();
		*this = std::move(tmp);
	}

	void TPath::MakeRelativeTo(const TPath& reference)
	{
		const usys_t n_strip = StripCommonPrefix(reference);
		const usys_t n_distinct = reference.components.Count() - n_strip;
		components.FillInsert(0, TPath::PARENT_DIR, n_distinct);
		cached_cstr.reset();
	}

	TPath::operator TString() const
	{
		if(components.Count() == 0)
			return TPath::CURRENT_DIR;
		return TBase::operator TString();
	}

	TPath::operator const char*() const
	{
		if(EL_UNLIKELY(cached_cstr == nullptr))
			cached_cstr = this->operator TString().MakeCStr();
		return cached_cstr.get();
	}

	TFile TPath::CreateAsFile(const bool recursive) const
	{
		if(recursive)
			Parent().CreateAsDirectory(true);

		return TFile(*this, TAccess::RW, ECreateMode::EXCLUSIVE);
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

	TPath::TPath(const TString& str) : TBase(str, SEPERATOR, true)
	{
		Validate();
	}

	TPath::TPath(const char* const str) : TPath(TString(str))
	{
	}

	TPath::TPath(const wchar_t* const str) : TPath(TString(str))
	{
	}

	TPath::TPath(std::initializer_list<TString> list) : TBase(list, SEPERATOR, true)
	{
		Validate();
	}

	TPath::TPath() : TBase(SEPERATOR, true)
	{
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

	TString TFile::ReadText(const TPath& path, const bool trim, const usys_t n_chars_max)
	{
		TString s = TFile(path).Pipe().Transform(text::encoding::utf8::TUTF8Decoder()).Limit(n_chars_max).Collect();
		if(trim)
			s.Trim();
		return s;
	}

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
