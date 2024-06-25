#pragma once
#include <memory>

#ifdef __linux__
	#define EL_OS_CLASS_POSIX
	#define EL_OS_LINUX
	#define EL_WCHAR_IS_UTF32
	#define EL_CHAR_IS_UTF8
#endif

#ifdef __GNUC__
	#ifdef __clang__
		#define EL_CC_CLANG
	#else
		#define EL_CC_GCC
	#endif
#endif

// only enable pure on g++, but NOT on clang++
// since clang++ does NOT allow pure functions to throw exceptions
#ifdef EL_CC_GCC
	#define EL_NO_SIDE_EFFECTS	__attribute__((pure))
	#define EL_NO_GLOBAL_MEMORY	__attribute__((const))
	#define	EL_LIKELY(expr)		__builtin_expect((expr), true)
	#define	EL_UNLIKELY(expr)	__builtin_expect((expr), false)
#else
	#define EL_NO_SIDE_EFFECTS
	#define EL_NO_GLOBAL_MEMORY
	#define	EL_LIKELY(expr)		(expr)
	#define	EL_UNLIKELY(expr)	(expr)
#endif

#if (defined EL_CC_GCC || defined EL_CC_CLANG)
	#define EL_WARN_UNUSED_RESULT __attribute__((warn_unused_result))
	#define EL_THREADLOCAL __thread
#else
	#define EL_WARN_UNUSED_RESULT
#endif

#if (defined EL_CC_GCC || defined EL_CC_CLANG)
	#define EL_PACKED __attribute__((packed))
#else
	#error "unsupported compiler"
#endif

#define EL_GETTER EL_NO_SIDE_EFFECTS EL_WARN_UNUSED_RESULT
#define EL_SETTER

namespace el1
{
	enum class EEndianity
	{
		LITTLE,
		BIG
	};

	static const EEndianity ENDIANITY = EEndianity::LITTLE;
	static const unsigned long PAGE_SIZE = 4096UL;

	template<typename T, typename C = T>
	inline std::unique_ptr<C> New()
	{
		return std::unique_ptr<C>(static_cast<C*>(new T()));
	}

	template<typename T, typename C = T, typename ... A>
	inline std::unique_ptr<C> New(A&& ... a)
	{
		return std::unique_ptr<C>(static_cast<C*>(new T(std::forward<A>(a) ...)));
	}
}
