#pragma once

#include "error.hpp"
#include "io_types.hpp"
#include "io_stream.hpp"
#include "io_collection_list.hpp"
#include "io_text_encoding.hpp"
#include "io_text_encoding_utf8.hpp"
#include "io_text_format.hpp"
#include "io_text_parser.hpp"
#include "io_text_scan.hpp"
#include "io_text_string.hpp"

#include <optional>
#include <tuple>
#include <type_traits>

namespace el1::io::text
{
	using namespace io::types;

	/** Text output target. Print() is the streaming equivalent of TString::Format(). */
	struct ITextWriter : format::IFormatSink
	{
		virtual ~ITextWriter() = default;

		ITextWriter& Write(const string::TStringView text)
		{
			Append(text.Data(), text.Length());
			return *this;
		}

		ITextWriter& Write(const string::TString& text) { return Write(text.View()); }

		template<typename... A>
		ITextWriter& Print(const format::TFormatString<std::type_identity_t<std::decay_t<const A>>...>& fmt, const A&... args)
		{
			fmt.RenderInto(*this, args...);
			return *this;
		}
	};

	class EL_LIFETIME_POINTER TStreamTextWriter final : public ITextWriter
	{
		stream::IBinarySink* const sink;
	public:
		explicit TStreamTextWriter(stream::IBinarySink* sink EL_LIFETIME_BOUND);
		void Append(const char32_t* data, usys_t length) final;
		void Flush() { sink->Flush(); }
	};

	/**
	 * Buffered text input with arbitrary lookahead. Scan() is type-safe and
	 * compile-time validates its native UTF-32 scan literal.
	 */
	struct ITextReader : parser::IInput
	{
		virtual ~ITextReader() = default;
		virtual void Consume(usys_t count) = 0;

		template<typename N>
		auto TryParse(const parser::TParser<N>& parser) -> std::optional<typename N::return_t>
		{
			usys_t consumed = 0;
			auto value = parser.TryParsePrefix(*this, consumed);
			if(value)
				Consume(consumed);
			return value;
		}

		template<typename N>
		auto Parse(const parser::TParser<N>& parser) -> typename N::return_t
		{
			auto value = TryParse(parser);
			EL_ERROR(!value, TException, "text input does not match parser");
			return std::move(*value);
		}

		template<typename... A>
		bool TryScan(const scan::TScanString<std::remove_cvref_t<A>...>& fmt, A&... output)
		{
			std::tuple<std::optional<std::remove_cvref_t<A>>...> values;
			usys_t consumed = 0;
			if(!fmt.TryScan(*this, values, consumed))
				return false;
			AssignTuple(values, output...);
			Consume(consumed);
			return true;
		}

		template<typename... A>
		usys_t Scan(const scan::TScanString<std::remove_cvref_t<A>...>& fmt, A&... output)
		{
			std::tuple<std::optional<std::remove_cvref_t<A>>...> values;
			usys_t consumed = 0;
			EL_ERROR(!fmt.TryScan(*this, values, consumed), TException, "text input does not match scan format");
			AssignTuple(values, output...);
			Consume(consumed);
			return consumed;
		}

	private:
		template<usys_t I = 0, typename TTuple, typename... A>
		static void AssignTuple(TTuple& values, A&... output)
		{
			if constexpr(I < sizeof...(A))
			{
				auto refs = std::tie(output...);
				std::get<I>(refs) = std::move(*std::get<I>(values));
				AssignTuple<I + 1>(values, output...);
			}
		}
	};

	class EL_LIFETIME_POINTER TStreamTextReader final : public ITextReader
	{
		struct TByteInput
		{
			stream::IBinarySource* source;
			byte_t value = 0;
			const byte_t* NextItem();
		};

		TByteInput byte_input;
		encoding::utf8::TUTF8Decoder decoder;
		io::collection::list::TList<char32_t> buffer;
		bool eof = false;

		bool Extend(usys_t offset);

	public:
		explicit TStreamTextReader(stream::IBinarySource* source EL_LIFETIME_BOUND);
		bool Peek(usys_t offset, char32_t& out) final;
		void Consume(usys_t count) final;
	};
}
