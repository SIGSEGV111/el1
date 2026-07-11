#pragma once

#include "io_collection_list.hpp"
#include "io_text_string.hpp"
#include <memory>

namespace el1::io::path
{
	using namespace io::types;
	using namespace io::text::encoding;
	using namespace io::text::string;
	using namespace io::collection::list;

	// A delimiter-separated hierarchy independent of filesystem semantics.
	class TPath
	{
		protected:
			TList<TString> components;
			mutable std::unique_ptr<char[]> cached_cstr;
			TUTF32 separator;
			bool allow_absolute;

			void Validate();
			void ValidateCompatibility(const TPath& other) const;

			TPath(TUTF32 separator, bool allow_absolute);
			TPath(const TString& str, TUTF32 separator, bool allow_absolute);
			TPath(std::initializer_list<TString> list, TUTF32 separator, bool allow_absolute);

		public:
			bool IsEmpty() const EL_GETTER { return components.Count() == 0; }

			TPath& operator+=(const TPath& rhs);
			TPath operator+(const TPath& rhs) const EL_GETTER;
			TPath& operator-=(const TPath& rhs);
			TPath operator-(const TPath& rhs) const EL_GETTER;

			bool operator==(const TPath& rhs) const EL_GETTER;
			bool operator!=(const TPath& rhs) const EL_GETTER;

			void Rebase(const TPath& old_prefix, const TPath& new_prefix);
			TPath Parent() const EL_GETTER;
			const array_t<const TString>& Components() const EL_GETTER { return components; }
			void ReplaceComponent(ssys_t index, const TPath& new_path);
			usys_t CountCommonPrefix(const TPath& other) const;
			usys_t StripCommonPrefix(const TPath& other);
			void Strip(usys_t n_components);
			void Truncate(usys_t n_components);

			bool IsAbsolute() const EL_GETTER;
			bool IsRelative() const EL_GETTER { return !IsAbsolute(); }
			// The delimiter used to split and render path components.
			TUTF32 Separator() const EL_GETTER { return separator; }

			operator TString() const EL_GETTER;
			operator const char*() const EL_GETTER;
			TString ToString() const EL_GETTER { return this->operator TString(); }

			TPath(TUTF32 separator = '/');
			TPath(const TString& str, TUTF32 separator);
			TPath(const char* str, TUTF32 separator);
			TPath(const wchar_t* str, TUTF32 separator);
			TPath(std::initializer_list<TString> list, TUTF32 separator);

			TPath(TPath&&) = default;
			TPath(const TPath& other);
			TPath& operator=(TPath&&) = default;
			TPath& operator=(const TPath& rhs);
	};

	struct TInvalidPathException : error::IException
	{
		enum class EReason
		{
			EMPTY_COMPONENT,
			EXPECTED_RELATIVE_PATH,
			NO_MATCH,
			EMPTY_PATH,
			COMPONENT_CONTAINS_SEPERATOR,
			POINTS_BEFORE_ROOT,
		};

		static const char* ReasonText(EReason reason);

		TPath path;
		usys_t idx_component;
		EReason reason;

		TString Message() const final override;
		error::IException* Clone() const override;

		TInvalidPathException(const TPath& path, usys_t idx_component, EReason reason) : path(path), idx_component(idx_component), reason(reason)
		{
		}
	};
}
