#include "error.hpp"
#include "io_text_string.hpp"
#include "io_file.hpp"

#include <iostream>

namespace el1::error
{
	using namespace io::types;
	using namespace io::text::string;
	using namespace io::file;

	TPath IException::File() const
	{
		TPath path = file;
		path.Simplify();
		return path;
	}

	void IException::Print(const char* const context_message) const
	{
		std::cerr<<"\nERROR: "<<context_message<<"\nMESSAGE: "<<Message().MakeCStr().get()<<"\nEXPRESSION: "<<(expression == nullptr ? "" : expression)<<"\nFILE: "<<(const char*)File()<<":"<<line<<"\nFUNCTION: "<<function<<'\n';

		const IException* e = nested.get();
		while(e != nullptr)
		{
			std::cerr<<"\nNESTED ERROR: "<<e->Message().MakeCStr().get()<<"\nEXPRESSION: "<<(e->expression == nullptr ? "" : e->expression)<<"\nFILE: "<<(const char*)e->File()<<":"<<e->line<<"\nFUNCTION: "<<e->function<<'\n';
			e = e->nested.get();
		}

		std::cerr<<'\n';
	}

	TString TException::Message() const
	{
		return *msg;
	}

	IException* TException::Clone() const
	{
		return new TException(*this);
	}

	TException::TException(const TString& msg) : msg(std::shared_ptr<TString>(new TString(msg)))
	{
	}

	TException::TException(TString&& msg) : msg(std::shared_ptr<TString>(new TString(std::move(msg))))
	{
	}

	const char* IException::what() const noexcept
	{
		if(msg_cstr.get() == nullptr)
		{
			TString str;

			for(const IException* current = this; current != nullptr; current = current->nested.get())
			{
				if(str.Length() > 0)
					str.chars.Append('\n');
				str += TString::Format("%s at %q:%d", current->Message(), current->File().operator TString(), current->line);
			}

			msg_cstr = std::shared_ptr<char[]>(str.MakeCStr().release());
		}
		return msg_cstr.get();
	}

	TString TUnknownException::Message() const
	{
		return "a unknown exception was caught";
	}

	IException* TUnknownException::Clone() const
	{
		return new TUnknownException(*this);
	}

	TString TOutOfMemoryException::Message() const
	{
		return TString::Format("out of memory (requested %d bytes of memory)", n_bytes_requested);
	}

	IException* TOutOfMemoryException::Clone() const
	{
		return new TOutOfMemoryException(*this);
	}

	TString TIndexOutOfBoundsException::Message() const
	{
		return TString::Format("index out of bounds (low-bound: %d, high_bound: %d, requested index: %d)", low_bound, high_bound, request_index);
	}

	IException* TIndexOutOfBoundsException::Clone() const
	{
		return new TIndexOutOfBoundsException(*this);
	}

	TString TNotImplementedException::Message() const
	{
		return TString::Format("function %q not implemented yet", function);
	}

	IException* TNotImplementedException::Clone() const
	{
		return new TNotImplementedException(*this);
	}

	TString TLogicException::Message() const
	{
		return TString("programm logic exception");
	}

	IException* TLogicException::Clone() const
	{
		return new TLogicException(*this);
	}

	TString TInvalidArgumentException::Message() const
	{
		return TString::Format("invalid argument value provided in %q: %s", argument_name, description);
	}

	IException* TInvalidArgumentException::Clone() const
	{
		return new TInvalidArgumentException(*this);
	}
}
