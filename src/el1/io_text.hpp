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

	/** Default binary cache size used by stream-backed text readers and writers. */
	inline constexpr usys_t DEFAULT_STREAM_TEXT_BUFFER_SIZE = 4096U;

	/** Text output target. Print() is the streaming equivalent of TString::Format(). */
	struct ITextWriter : format::IFormatSink
	{
		virtual ~ITextWriter() = default;

		ITextWriter& Write(const string::TStringView text)
		{
			Append(text.Data(), text.Length());
			return *this;
		}

		template<typename... A>
		ITextWriter& Print(const format::TFormatString<std::type_identity_t<std::decay_t<const A>>...>& fmt, const A&... args)
		{
			fmt.RenderInto(*this, args...);
			return *this;
		}
	};

	class EL_LIFETIME_POINTER TStreamTextWriter final : public ITextWriter
	{
		struct TCharInput
		{
			const char32_t* data = nullptr;
			usys_t count = 0;
			usys_t pos = 0;

			const char32_t* NextItem() noexcept { return pos < count ? data + pos++ : nullptr; }
		};

		stream::IBinarySink* const sink;
		const usys_t buffer_size;
		io::collection::list::TList<byte_t> buffer;
		usys_t n_buffered = 0;
		encoding::utf8::TUTF8Encoder encoder;

		void FlushBuffer();

	public:
		/**
		 * Buffered UTF-8 writer. buffer_size is the binary output cache size in bytes.
		 * Call Flush() before writing directly to sink so buffered text cannot be overtaken.
		 */
		explicit TStreamTextWriter(stream::IBinarySink* sink EL_LIFETIME_BOUND, usys_t buffer_size = DEFAULT_STREAM_TEXT_BUFFER_SIZE);
		~TStreamTextWriter() final;
		void Append(const char32_t* data, usys_t length) final;
		void Flush();
		usys_t BufferSize() const noexcept { return buffer_size; }
	};

	struct text_position_t
	{
		iosize_t character_index;
		iosize_t line_index;
	};

	/**
	 * Buffered text input with arbitrary lookahead. CharacterIndex() and
	 * LineIndex() refer to the next unconsumed character and are zero-based.
	 */
	struct ITextReader : stream::IBufferedSource<char32_t>
	{
		private:
			iosize_t character_index = 0;
			iosize_t line_index = 0;

		protected:
			virtual bool DoEnsure(usys_t count) = 0;
			virtual void DoShift(usys_t count) = 0;

		public:
			virtual ~ITextReader() = default;

			iosize_t CharacterIndex() const noexcept { return character_index; }
			iosize_t LineIndex() const noexcept { return line_index; }

			bool Ensure(const usys_t count) final override
			{
				if(!DoEnsure(count))
					return false;
				return Head().Count() >= count;
			}

			/** Resolve a lookahead offset to an absolute character/line position. */
			text_position_t Position(usys_t offset = 0)
			{
				if(!Ensure(offset))
				{
					offset = util::Min(offset, Count());
					EL_ERROR(!Ensure(offset), TLogicException);
				}

				iosize_t line = line_index;
				const auto head = Head();
				for(usys_t i = 0; i < offset; i++)
					if(head[i] == U'\n')
						line++;
				return {character_index + (iosize_t)offset, line};
			}

			void Shift(const usys_t count) final override
			{
				if(count == 0)
					return;
				EL_ERROR(!Ensure(count), stream::TStreamDryException);

				iosize_t lines = 0;
				const auto head = Head();
				for(usys_t i = 0; i < count; i++)
					if(head[i] == U'\n')
						lines++;
				DoShift(count);

				character_index += (iosize_t)count;
				line_index += lines;
			}

			template<typename N>
			auto TryParse(const parser::TParser<N>& grammar, const parser::TParseLimits limits = {}) -> std::optional<typename N::return_t>
			{
				usys_t consumed = 0;
				auto value = grammar.TryParsePrefix(*this, consumed, limits);
				if(value)
					Shift(consumed);
				return value;
			}

			template<typename N>
			auto Parse(const parser::TParser<N>& grammar, const parser::TParseLimits limits = {}) -> typename N::return_t
			{
				auto value = TryParse(grammar, limits);
				EL_ERROR(!value, TException, U"text input does not match parser");
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
				Shift(consumed);
				return true;
			}

			template<typename... A>
			usys_t Scan(const scan::TScanString<std::remove_cvref_t<A>...>& fmt, A&... output)
			{
				std::tuple<std::optional<std::remove_cvref_t<A>>...> values;
				usys_t consumed = 0;
				EL_ERROR(!fmt.TryScan(*this, values, consumed), TException, U"text input does not match scan format");
				AssignTuple(values, output...);
				Shift(consumed);
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

	/** Non-owning text reader over a TStringView. */
	class EL_LIFETIME_POINTER TStringViewTextReader final : public ITextReader
	{
		io::collection::array::TArraySource<char32_t> source;
	protected:
		bool DoEnsure(const usys_t count) final { return source.Ensure(count); }
		void DoShift(const usys_t count) final { source.Shift(count); }
	public:
		explicit TStringViewTextReader(const string::TStringView string EL_LIFETIME_BOUND) : source(static_cast<const io::collection::array::array_t<const char32_t>&>(string)) {}
		usys_t Count() const noexcept final { return source.Count(); }
		const char32_t& operator[](const usys_t index) const final { return source[index]; }
		io::collection::array::array_t<const char32_t> Head() const noexcept EL_LIFETIME_BOUND final { return source.Head(); }
	};

	/** Owning text reader for an in-memory TString/TStringView. */
	class TStringTextReader final : public ITextReader
	{
		string::TStringSource source;
	protected:
		bool DoEnsure(const usys_t count) final { return source.Ensure(count); }
		void DoShift(const usys_t count) final { source.Shift(count); }
	public:
		explicit TStringTextReader(string::TString string) : source(std::move(string)) {}
		explicit TStringTextReader(const string::TStringView string) : source(string) {}
		usys_t Count() const noexcept final { return source.Count(); }
		const char32_t& operator[](const usys_t index) const final { return source[index]; }
		io::collection::array::array_t<const char32_t> Head() const noexcept EL_LIFETIME_BOUND final { return source.Head(); }
	};

	/** Owning text reader for an in-memory TList<char32_t>. */
	class TListTextReader final : public ITextReader
	{
		io::collection::list::TListSource<char32_t> source;
	protected:
		bool DoEnsure(const usys_t count) final { return source.Ensure(count); }
		void DoShift(const usys_t count) final { source.Shift(count); }
	public:
		explicit TListTextReader(io::collection::list::TList<char32_t> chars) : source(std::move(chars)) {}
		usys_t Count() const noexcept final { return source.Count(); }
		const char32_t& operator[](const usys_t index) const final { return source[index]; }
		io::collection::array::array_t<const char32_t> Head() const noexcept EL_LIFETIME_BOUND final { return source.Head(); }
	};

	class EL_LIFETIME_POINTER TStreamTextReader final : public ITextReader
	{
		struct TByteInput
		{
			stream::IBinarySource* source;
			io::collection::list::TList<byte_t> buffer;
			usys_t pos = 0;
			usys_t count = 0;

			TByteInput(stream::IBinarySource* source, usys_t buffer_size);
			const byte_t* NextItem();
		};

		const usys_t buffer_size;
		const usys_t decode_ahead;
		TByteInput byte_input;
		encoding::utf8::TUTF8Decoder decoder;
		io::collection::list::TList<char32_t> buffer;
		usys_t pos = 0;
		bool eof = false;

	protected:
		bool DoEnsure(usys_t count) final;
		void DoShift(usys_t count) final;

	public:
		/**
		 * Buffered UTF-8 reader. buffer_size is the binary input cache size in bytes;
		 * decoded text is read ahead by roughly the same amount of memory. Read-ahead
		 * advances source, so do not read source directly while this reader is active.
		 */
		explicit TStreamTextReader(stream::IBinarySource* source EL_LIFETIME_BOUND, usys_t buffer_size = DEFAULT_STREAM_TEXT_BUFFER_SIZE);
		usys_t Count() const noexcept final { return buffer.Count() - pos; }
		const char32_t& operator[](usys_t index) const final;
		io::collection::array::array_t<const char32_t> Head() const noexcept EL_LIFETIME_BOUND final;
		usys_t BufferSize() const noexcept { return buffer_size; }
	};
}
