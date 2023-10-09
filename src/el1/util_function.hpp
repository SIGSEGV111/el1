#pragma once

#include <memory>
#include <type_traits>

namespace el1::util::function
{
	template<typename R, typename ...A>
	struct IFunction
	{
		virtual R operator()(A ...a) const = 0;
		virtual ~IFunction() {}
	};

	template<typename T, typename R, typename ...A>
	class TMemberFunction : public IFunction<R, A...>
	{
		public:
			using function_ptr_t = R (T::*)(A... a);

		protected:
			T* const object;
			function_ptr_t function;

		public:
			R operator()(A ...a) const final override { return (this->object->*function)(a...); }
			TMemberFunction(T* object, function_ptr_t function) : object(object), function(function) {}
	};

	template<typename R, typename ...A>
	class TBasicFunction : public IFunction<R, A...>
	{
		public:
			using function_ptr_t = R (*)(A... a);

		protected:
			function_ptr_t function;

		public:
			R operator()(A ...a) const final override { return function(a...); }
			TBasicFunction(function_ptr_t function) : function(function) {}
	};

	template<typename L, typename R, typename ...A>
	class TLambdaFunction : public IFunction<R, A...>
	{
		protected:
			L lambda;

		public:
			R operator()(A ...a) const final override { return lambda(a...); }
			TLambdaFunction(L lambda) : lambda(std::move(lambda)) {}
	};

	template<typename R, typename ...A>
	class TFunction
	{
		protected:
			std::shared_ptr< IFunction<R, A...> > function;

		public:
			TFunction() = default;
			TFunction(const TFunction& other) = default;
			TFunction(TFunction&&) = default;
			TFunction& operator=(TFunction&& rhs) = default;
			TFunction& operator=(const TFunction& rhs) = default;

			bool operator==(const TFunction& rhs) const { return function.get() == rhs.function.get(); }
			R operator()(A... a) const { return (*function)(a...); }

			TFunction(R (*function)(A... a)) : function(new TBasicFunction<R, A...>(function)) {}

			template<typename T>
			TFunction(T* const object, R (T::*function)(A... a)) : function(new TMemberFunction<T, R, A...>(object, function)) {}

			template<typename L>
			TFunction(L lambda) : function(new TLambdaFunction<L, R, A...>(std::move(lambda))) {}

	};
}
