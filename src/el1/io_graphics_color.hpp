#pragma once
#include "io_types.hpp"
#include "math_vector.hpp"

namespace el1::io::graphics::color
{
	using namespace io::types;

	template<typename T>
	struct rgb_t : math::vector::TVector<T, 3>
	{
		using math::vector::TVector<T, 3>::v;

		inline T& Red()   { return v[0]; }
		inline T& Green() { return v[1]; }
		inline T& Blue()  { return v[2]; }

		inline const T& Red()   const { return v[0]; }
		inline const T& Green() const { return v[1]; }
		inline const T& Blue()  const { return v[2]; }

		constexpr rgb_t() {}
		constexpr rgb_t(const T red, const T green, const T blue) : math::vector::TVector<T, 3>(red, green, blue) {}
	};

	template<typename T>
	struct rgba_t : math::vector::TVector<T, 4>
	{
		using math::vector::TVector<T, 4>::v;

		inline T& Red()   { return v[0]; }
		inline T& Green() { return v[1]; }
		inline T& Blue()  { return v[2]; }
		inline T& Alpha() { return v[3]; }

		inline const T& Red()   const { return v[0]; }
		inline const T& Green() const { return v[1]; }
		inline const T& Blue()  const { return v[2]; }
		inline const T& Alpha() const { return v[3]; }

		operator rgb_t<T>&() { return *reinterpret_cast<rgb_t<T>*>(this); }
		operator const rgb_t<T>&() const { return *reinterpret_cast<const rgb_t<T>*>(this); }

		constexpr rgba_t() {}
		constexpr rgba_t(const T red, const T green, const T blue, const T alpha) : math::vector::TVector<T, 4>(red, green, blue, alpha) {}
	};

	using rgb8_t  = rgb_t<u8_t>;
	using rgb16_t = rgb_t<u16_t>;
	using rgbf_t  = rgb_t<float>;
}
