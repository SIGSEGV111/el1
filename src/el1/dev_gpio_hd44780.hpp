#pragma once

#include "dev_gpio.hpp"

namespace el1::dev::gpio::hd44780
{
	static const bool DEBUG = false;

	class THD44780
	{
		protected:
			std::unique_ptr<IPin> rs;	// register select (low = command register; high = data register)
			std::unique_ptr<IPin> rw;	// read/write select (low = write; high = read)
			std::unique_ptr<IPin> e;	// start signal
			std::unique_ptr<IPin> bl;
			std::unique_ptr<IPin> data[8];

			void Commit();
			void Poll();
			void WaitWhileBusy();
			void SetRaw(const bool rs, const u8_t data);
			u8_t GetRaw(const bool rs);
			void SendCommand(const u8_t cmd);
			void SendData(const u8_t data);
			u8_t GetAddress();
			void SetAddress(const u8_t addr);

			u8_t dram_state[80];
			u8_t dram_new[80];

			static u8_t TranslateCharToDram(const TUTF32 chr);
			static TUTF32 TranslateDramToChar(const u8_t dram);
			static u8_t CoordinateToAddress(const unsigned x, const unsigned y);

		public:
			void Render();
			void Text(const TString str);

			TUTF32 Character(const unsigned x, const unsigned y) const EL_GETTER;
			void Character(const unsigned x, const unsigned y, const TUTF32 chr) EL_SETTER;

			void Backlight(const bool) EL_SETTER;
			bool Backlight() const EL_GETTER;

			// "bl" is backlight control and can be nullptr
			THD44780(std::unique_ptr<IPin> rs, std::unique_ptr<IPin> rw, std::unique_ptr<IPin> e, std::initializer_list<std::unique_ptr<IPin>> data, std::unique_ptr<IPin> bl);
			~THD44780();
	};
}
