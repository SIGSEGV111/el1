#include "dev_spi_led_neopixel.hpp"

namespace el1::dev::spi::led::neopixel
{
	TColorComponent::operator u8_t() const
	{
		u8_t r = 0;
		r |= ((this->b[0] & 0b01000000) << 1) | ((this->b[0] & 0b00001000) << 3) | ((this->b[0] & 0b00000001) << 5);
		r |= ((this->b[1] & 0b00100000) >> 1) | ((this->b[1] & 0b00000100) << 1);
		r |= ((this->b[2] & 0b10000000) >> 5) | ((this->b[2] & 0b00010000) >> 3) | ((this->b[2] & 0b00000010) >> 1);
		return r;
	}

	TColorComponent& TColorComponent::operator=(const u8_t v)
	{
		this->b[0] = (u8_t)(0b10010010 | ((v & 0b10000000) >> 1) | ((v & 0b01000000) >> 3) | ((v & 0b00100000) >> 5));
		this->b[1] = (u8_t)(0b01001001 | ((v & 0b00010000) << 1) | ((v & 0b00001000) >> 1));
		this->b[2] = (u8_t)(0b00100100 | ((v & 0b00000100) << 5) | ((v & 0b00000010) << 3) | ((v & 0b00000001) << 1));
		return *this;
	}

	TColorComponent::TColorComponent(const u8_t init)
	{
		(*this) = init;
	}
}
