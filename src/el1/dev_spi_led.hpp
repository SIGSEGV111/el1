#pragma once
#include "dev_spi.hpp"
#include "io_collection_list.hpp"
#include "io_graphics_color.hpp"
#include "math.hpp"

namespace el1::dev::spi::led
{
	// Everything begins with a LED strip - no matter how you layout the LEDs physically, they
	// are always linked as a strip electronically. Changes made to the in-memory data is
	// not immediately applied to the LEDs, instead one has to call the `Apply()` function to update the LEDs.
	template<typename TSpiLed>
	class TLedStrip
	{
		public:
			using color_t = typename TSpiLed::color_t;

		protected:
			std::unique_ptr<ISpiDevice> dev;
			io::collection::list::TList<TSpiLed> leds;
			int TestNumLeds(const unsigned num_leds) const;

		public:
			io::collection::list::array_t<TSpiLed> Leds() EL_GETTER;

			// sets the color of all LEDs on the srip
			void UnifiedColor(const color_t) EL_SETTER;

			// updates the physical LEDs
			void Apply();

			/**
			 * @param dev The SPI device which connects to the LEDs.
			 * Connect the SPI-MOSI pin through a level-shifter to the input of the first LED.
			 * (Optional) Connect the output of the last LED through a level-shifter to the SPI-MISO pin.
			 * Connect the SPI-CE pin to your level-shifter(s) output-enable pin.
			 * @param num_leds The number of LEDs on the strip.
			 * Use -1 to auto-detect. Auto-detection only works with MISO connected to the last LED - see above.
			 */
			TLedStrip(std::unique_ptr<ISpiDevice> dev, int num_leds);
	};

	struct IVirtualLed
	{
		using color_t = io::graphics::color::rgbf_t;
		virtual color_t EffectiveColor() const EL_GETTER = 0;
		virtual void EffectiveColor(const color_t) EL_SETTER = 0;
	};

	template<typename TSpiLed>
	class TLedMatrix : public IVirtualLed
	{
		public:
			using color_t = io::graphics::color::rgbf_t;
			using index_t = math::vector::TVector<s16_t, 2>;

		protected:
			io::collection::list::array_t<TSpiLed> leds;

		public:
			color_t EffectiveColor() const final override EL_GETTER;
			void EffectiveColor(const color_t) final override EL_SETTER;
			TSpiLed& operator[](const index_t) EL_GETTER;
			TLedMatrix(io::collection::list::array_t<TSpiLed> leds);
	};

	template<typename TSpiLed>
	class TLedCylinder : public IVirtualLed
	{
		public:
			using color_t = io::graphics::color::rgbf_t;

		protected:
			io::collection::list::array_t<TSpiLed> leds;

		public:
			color_t EffectiveColor() const final override EL_GETTER;
			void EffectiveColor(const color_t) final override EL_SETTER;
			TLedCylinder(io::collection::list::array_t<TSpiLed> leds, const float num_leds_per_revolution);
	};

	/********************************************************************/

	template<typename TSpiLed>
	void TLedStrip<TSpiLed>::UnifiedColor(const color_t new_color)
	{
		for(auto& led : leds)
			led.Color(new_color);
	}

	template<typename TSpiLed>
	void TLedStrip<TSpiLed>::Apply()
	{
		dev->ExchangeBuffers(&leds[0], nullptr, sizeof(TSpiLed) * leds.Count(), true);
	}

	template<typename TSpiLed>
	TLedStrip<TSpiLed>::TLedStrip(std::unique_ptr<ISpiDevice> _dev, int num_leds) : dev(std::move(_dev))
	{
		EL_ERROR(num_leds < -1, TInvalidArgumentException, "num_leds", "num_leds must be >= -1");
		dev->Clock(TSpiLed::HZ_SPI);

		if(num_leds == -1)
		{
			int c;
			num_leds = util::BinarySearch([&](auto n) { return c = TestNumLeds(n); }, 1024);
			if(c > 0) num_leds--;
		}

		leds.FillInsert(0, TSpiLed(), num_leds);
	}

	template<typename TSpiLed>
	int TLedStrip<TSpiLed>::TestNumLeds(const unsigned num_leds) const
	{
		const byte_t zero[sizeof(TSpiLed)] = {};
		TSpiLed arr[num_leds + 1];
		dev->ExchangeBuffers(arr, arr, sizeof(arr), true);
		return memcmp(arr + num_leds, zero, sizeof(TSpiLed)) == 0 ? -1 : 1;
	}
}
