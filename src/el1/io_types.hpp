#pragma once

#include <type_traits>

namespace el1::io::types
{
	using s8_t = signed char;
	using u8_t = unsigned char;
	using s16_t = signed short int;
	using u16_t = unsigned short int;
	using s32_t = signed int;
	using u32_t = unsigned int;
	using s64_t = signed long long;
	using u64_t = unsigned long long;
	using iosize_t = u64_t;
	using siosize_t = s64_t;

	using byte_t = u8_t;

	using f32_t = float;
	using f64_t = double;

	using ssys_t = std::conditional<sizeof(void*) == sizeof(s32_t), s32_t, s64_t>::type;
	using usys_t = std::conditional<sizeof(void*) == sizeof(u32_t), u32_t, u64_t>::type;
	using fsys_t = std::conditional<sizeof(void*) == sizeof(f32_t), f32_t, f64_t>::type;

	static const usys_t NEG1 = (usys_t)-1;

	// gets thrown when a thread or fiber is supposed to shutdown
	struct shutdown_t {};

	namespace traits
	{
		// copied from somewhere... unknown author
		// if you are the author let me know and I add you to the credits or remove the code

		template < std::size_t, typename... > struct type_at ;

		template < std::size_t N, typename FIRST, typename... REST >
		struct type_at<N,FIRST,REST...> : type_at< N-1, REST... > {} ;

		template < typename FIRST, typename... REST >
		struct type_at<0,FIRST,REST...> { using type = FIRST ; } ;
	}

	template<typename TKey, typename TValue>
	struct kv_pair_tt
	{
		TKey key;
		TValue value;
	};
}
