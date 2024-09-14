#pragma once
#include "def.hpp"
#include "io_types.hpp"
#include "io_graphics_color.hpp"

namespace el1::dev::spi::led::neopixel
{
	using namespace io::types;

	class TColorComponent
	{
		protected:
			byte_t b[3];

		public:
			operator u8_t() const EL_GETTER;
			TColorComponent& operator=(const u8_t) EL_SETTER;
			TColorComponent(const u8_t init = 0);
	} __attribute__((packed));

	using color_t = io::graphics::color::rgb8_t;

	template<unsigned idx_red, unsigned idx_green, unsigned idx_blue>
	class TNeoPixel
	{
		public:
			using color_t = io::graphics::color::rgb8_t;
			static const u64_t HZ_SPI = 2500000;	// this yields 400ns bit-times on the bus

		protected:
			TColorComponent cc[3];

		public:
			color_t Color() const EL_GETTER;
			void Color(const color_t) EL_SETTER;
	} __attribute__((packed));

	using TWS2812B = TNeoPixel<1,0,2>;
	using TWS2811  = TNeoPixel<0,1,2>;

	/****************************************************************/

	// template<unsigned idx_red, unsigned idx_green, unsigned idx_blue>
	// unsigned long TNeoPixel<idx_red, idx_green, idx_blue>::HZ_SPI = 2500000;	// this yields 400ns bit-times on the bus

	template<unsigned idx_red, unsigned idx_green, unsigned idx_blue>
	color_t TNeoPixel<idx_red, idx_green, idx_blue>::Color() const
	{
		return { cc[idx_red], cc[idx_green], cc[idx_blue] };
	}

	template<unsigned idx_red, unsigned idx_green, unsigned idx_blue>
	void TNeoPixel<idx_red, idx_green, idx_blue>::Color(const color_t new_color)
	{
		cc[idx_red] = new_color[0];
		cc[idx_green] = new_color[1];
		cc[idx_blue] = new_color[2];
	}
}
