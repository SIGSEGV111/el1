#pragma once

#include "io_types.hpp"
#include "def.hpp"
#include <exception>
#include <memory>

namespace el1::io::text::string
{
	class TString;
}

namespace el1::io::file
{
	class TPath;
}

namespace el1::error
{
	using namespace io::types;
	using namespace io::text::string;

	struct IException : std::exception
	{
		const char* expression;
		const char* file;
		const char* function;
		unsigned line;
		std::shared_ptr<IException> nested;
		mutable std::shared_ptr<char[]> msg_cstr;

		io::file::TPath File() const;

		virtual TString Message() const = 0;
		virtual IException* Clone() const = 0;

		void Configure(IException* nested, const char* const expression, const char* const file, unsigned line, const char* const function)
		{
			this->nested = std::shared_ptr<IException>(nested);
			this->expression = expression;
			this->file = file;
			this->function = function;
			this->line = line;
		}

		const char* what() const noexcept final override;

		void Print(const char* const context_message) const;

		IException() : expression(nullptr), file(nullptr), function(nullptr), line(0) {}
		virtual ~IException() = default;
	};

	struct TException : IException
	{
		const std::shared_ptr<TString> msg;

		TString Message() const final override;
		IException* Clone() const override;
		TException(const TString& msg);
		TException(TString&& msg);
	};

	struct TUnknownException : IException
	{
		TString Message() const final override;
		IException* Clone() const override;
	};

	struct TOutOfMemoryException : IException
	{
		const ssys_t n_bytes_requested;

		TString Message() const final override;
		IException* Clone() const override;

		TOutOfMemoryException(const ssys_t n_bytes_requested) : n_bytes_requested(n_bytes_requested)
		{
		}
	};

	struct TIndexOutOfBoundsException : IException
	{
		const ssys_t low_bound;
		const ssys_t high_bound;
		const ssys_t request_index;

		TString Message() const final override;
		IException* Clone() const override;

		TIndexOutOfBoundsException(const ssys_t low_bound, const ssys_t high_bound, const ssys_t request_index) : low_bound(low_bound), high_bound(high_bound), request_index(request_index)
		{
		}
	};

	struct TSyscallException : IException
	{
		const int error_code;
		TSyscallException(const int error_code) : error_code(error_code) {}

		TString Message() const final override;
		IException* Clone() const override;

		TSyscallException();
	};

	struct TNotImplementedException : IException
	{
		TString Message() const final override;
		IException* Clone() const override;
	};

	struct TLogicException : IException
	{
		TString Message() const final override;
		IException* Clone() const override;
	};

	struct TInvalidArgumentException : IException
	{
		const char* const argument_name;
		const char* const description;

		TString Message() const final override;
		IException* Clone() const override;

		TInvalidArgumentException(const char* const argument_name, const char* const description) : argument_name(argument_name), description(description) {}
	};

	template<typename E>
	static void Throw [[ noreturn ]] (IException* nested, const char* const expression, const char* const file, const unsigned line, const char* const function)
	{
		E e;
		e.Configure(nested, expression, file, line, function);
		throw e;
	}

	template<typename E, typename ...A>
	static void Throw [[ noreturn ]] (IException* nested, const char* const expression, const char* const file, const unsigned line, const char* const function, A&&... a)
	{
		E e(a...);
		e.Configure(nested, expression, file, line, function);
		throw e;
	}

	template<typename E>
	static void Warning(IException* nested, const char* const expression, const char* const file, const unsigned line, const char* const function)
	{
		E e;
		e.Configure(nested, expression, file, line, function);
		e.Print("WARNING");
	}

	template<typename E, typename ...A>
	static void Warning(IException* nested, const char* const expression, const char* const file, const unsigned line, const char* const function, A&&... a)
	{
		E e(a...);
		e.Configure(nested, expression, file, line, function);
		e.Print("WARNING");
	}

	#define EL_THROW(type, ...) do { using namespace el1::error; Throw<type>(nullptr, nullptr, __FILE__, __LINE__, __func__ __VA_OPT__(,) __VA_ARGS__); } while(false)

	#define EL_FORWARD(nested, type, ...) do { using namespace el1::error; Throw<type>((nested).Clone(), nullptr, __FILE__, __LINE__, __func__ __VA_OPT__(,) __VA_ARGS__); } while(false)

	#define EL_ANNOTATE_ERROR(expression, type, ...) (([&](const char* const funcname) -> decltype(((expression))) { using namespace el1::error; try { return ((expression)); } catch(const IException& nested) { Throw<type>(nested.Clone(), #expression, __FILE__, __LINE__, funcname __VA_OPT__(,) __VA_ARGS__); } })(__func__))

	#define EL_ERROR(condition, type, ...) do { if( EL_UNLIKELY((condition)) ) { using namespace el1::error; Throw<type>(nullptr, #condition, __FILE__, __LINE__, __func__ __VA_OPT__(,) __VA_ARGS__); } } while(false)

	#define EL_WARN(condition, type, ...) do { if( EL_UNLIKELY((condition)) ) { using namespace el1::error; Warning<type>(nullptr, #condition, __FILE__, __LINE__, __func__ __VA_OPT__(,) __VA_ARGS__); } } while(false)

	#define EL_NOT_IMPLEMENTED EL_THROW(TNotImplementedException)


	#ifdef EL_OS_CLASS_POSIX
		#define EL_SYSERR(syscall) (([&](const char* const funcname){ \
			using namespace el1::error; \
			const auto __syscall_result = ((syscall)); \
			if( EL_UNLIKELY((long)__syscall_result == -1L) ) \
				Throw<TSyscallException>(nullptr, #syscall, __FILE__, __LINE__, funcname); \
			return __syscall_result; \
		})(__func__))

		struct TPthreadException : IException
		{
			const int error_code;

			TString Message() const final override;
			IException* Clone() const override;

			TPthreadException(int error_code) : error_code(error_code) {}
		};

		#define EL_PTHREAD_ERROR(pth_call) do { const int __pth_error_code = (pth_call); EL_ERROR(__pth_error_code != 0, TPthreadException, __pth_error_code); } while(false)
	#endif
}
