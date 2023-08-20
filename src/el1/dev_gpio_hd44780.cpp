#include "dev_gpio_hd44780.hpp"
#include "debug.hpp"

#include <stdio.h>
#include <unistd.h>

namespace el1::dev::gpio::hd44780
{
	void THD44780::Commit()
	{
		this->rs->Controller()->Commit();
	}

	void THD44780::Poll()
	{
		this->rs->Controller()->Poll();
	}

	void THD44780::WaitWhileBusy()
	{
		this->rs->State(false);
		this->rw->State(true);
		this->e->State(false);

		for(unsigned i = 0; i < 8; i++)
			this->data[i]->Mode(EMode::INPUT);

		this->data[7]->Trigger(ETrigger::FALLING_EDGE);
		Commit();

		this->e->State(true);
		Commit();

		Poll();
		while(this->data[7]->State())
			EL_ERROR(!this->data[7]->OnInputTrigger().WaitFor(0.1), TException, "HD44780 display did not leave busy state within 100ms");

		this->data[7]->Trigger(ETrigger::DISABLED);
		this->e->State(false);
		this->rw->State(false);
		Commit();
	}

	static const TTime T_ENABLE_PULSE = TTime(1.0 / 1000000.0);

	void THD44780::SetRaw(const bool rs, const u8_t data)
	{
		this->rs->State(rs);
		this->rw->State(false);
		this->e->State(false);

		for(unsigned i = 0; i < 8; i++)
		{
			this->data[i]->Mode(EMode::OUTPUT);
			this->data[i]->State((data & (1<<i)) != 0);
		}

		Commit();

		this->e->State(true);
		Commit();

		TFiber::Sleep(T_ENABLE_PULSE);

		this->e->State(false);
		Commit();
	}

	u8_t THD44780::GetRaw(const bool rs)
	{
		this->rs->State(rs);
		this->rw->State(true);
		this->e->State(false);

		for(unsigned i = 0; i < 8; i++)
		{
			this->data[i]->Mode(EMode::INPUT);
		}

		Commit();

		this->e->State(true);
		Commit();

		TFiber::Sleep(T_ENABLE_PULSE);

		Poll();
		u8_t data = 0;
		for(unsigned i = 0; i < 8; i++)
		{
			data |= ((this->data[i]->State() ? 1 : 0) << i);
		}

		this->e->State(false);
		Commit();

		return data;
	}

	void THD44780::SendCommand(const u8_t cmd)
	{
		SetRaw(false, cmd);
		WaitWhileBusy();
	}

	u8_t THD44780::GetAddress()
	{
		const u8_t real_address = GetRaw(false) & 0b01111111;
		const u8_t addr = real_address >= 64 ? real_address - 24 : real_address;
		EL_ERROR(!DEBUG && addr >= 80, TException, TString::Format("HD44780 returned invalid DRAM address %d", addr));
		return addr;
	}

	void THD44780::SetAddress(const u8_t addr)
	{
		if(DEBUG) fprintf(stderr, "THD44780::SetAddress(const u8_t addr = %hhd)\n", addr);
		EL_ERROR(!DEBUG && addr >= 80, TInvalidArgumentException, "addr", "addr must be < 80");

		const u8_t real_address = addr >= 40 ? addr + 24 : addr;

		SendCommand(0b10000000 | real_address); // set DRAM address

		if(DEBUG)
		{
			u8_t ret_addr;
			EL_WARN((ret_addr = GetAddress()) != addr, TException, TString::Format("failed to set DRAM address %d (LCD returned address %d)", addr, ret_addr));
		}
	}

	void THD44780::SendData(const u8_t data)
	{
		SetRaw(true, data);
		WaitWhileBusy();
	}

	static const u8_t LINE_TO_DRAM_ADDR[] = {
		0,
		40,
		20,
		60
	};

	static const u8_t LINE_LENGTH = 20;
	static const u8_t LINE_COUNT = sizeof(LINE_TO_DRAM_ADDR) / sizeof(LINE_TO_DRAM_ADDR[0]);

	void THD44780::Render()
	{
		if(DEBUG) debug::Hexdump(dram_state, sizeof(dram_state), LINE_LENGTH);

		u8_t dram_addr = (u8_t)-1;
		for(u8_t i = 0; i < sizeof(dram_state); i++)
			if(dram_new[i] != dram_state[i])
			{
				if(dram_addr != i)
				{
					SetAddress(i);
					dram_addr = i;
				}

				SendData(dram_new[i]);
				dram_state[i] = dram_new[i];
				dram_addr++;
			}

		if(DEBUG) debug::Hexdump(dram_state, sizeof(dram_state), LINE_LENGTH);
	}

	u8_t THD44780::CoordinateToAddress(const unsigned x, const unsigned y)
	{
		EL_ERROR(!DEBUG && y >= LINE_COUNT, TInvalidArgumentException, "y", "y out of bounds");
		EL_ERROR(!DEBUG && x >= LINE_LENGTH, TInvalidArgumentException, "x", "x out of bounds");
		const u8_t addr = LINE_TO_DRAM_ADDR[y] + x;
		if(DEBUG) fprintf(stderr, "THD44780::CoordinateToAddress(const unsigned x = %d, const unsigned y = %d) => %hhd\n", x, y, addr);
		return addr;
	}

	TUTF32 THD44780::Character(const unsigned x, const unsigned y) const
	{
		const u8_t dram_addr = CoordinateToAddress(x, y);
		return TranslateDramToChar(dram_new[dram_addr]);
	}

	void THD44780::Character(const unsigned x, const unsigned y, const TUTF32 chr)
	{
		const u8_t dram_addr = CoordinateToAddress(x, y);
		dram_new[dram_addr] = TranslateCharToDram(chr);
	}

	void THD44780::Text(const TString str)
	{
		unsigned x = 0;
		unsigned y = 0;

		memset(dram_new, TranslateCharToDram(' '), sizeof(dram_new));

		for(TUTF32 chr : str.chars)
		{
			if(chr == '\n')
			{
				x = 0;
				y++;
			}
			else
			{
				if(x >= LINE_LENGTH)
				{
					x = 0;
					y++;
				}

				Character(x, y, chr);
				x++;
			}
		}

		Render();
	}

	void THD44780::Backlight(const bool new_state)
	{
		if(this->bl)
			this->bl->State(new_state);
	}

	bool THD44780::Backlight() const
	{
		if(this->bl)
			return this->bl->State();
		else
			return true;
	}

	THD44780::THD44780(std::unique_ptr<IPin> rs, std::unique_ptr<IPin> rw, std::unique_ptr<IPin> e, std::initializer_list<std::unique_ptr<IPin>> _data, std::unique_ptr<IPin> bl) : rs(std::move(rs)), rw(std::move(rw)), e(std::move(e)), bl(std::move(bl))
	{
		array_t<std::unique_ptr<IPin>> data((std::unique_ptr<IPin>*)_data.begin(), _data.end() - _data.begin());
		for(unsigned i = 0; i < 8; i++)
		{
			this->data[i] = std::move(data[i]);
			this->data[i]->AutoCommit(false);
			this->data[i]->Mode(EMode::OUTPUT);
		}

		this->rs->AutoCommit(false);
		this->rs->Mode(EMode::OUTPUT);
		this->rw->AutoCommit(false);
		this->rw->Mode(EMode::OUTPUT);
		this->e->AutoCommit(false);
		this->e->Mode(EMode::OUTPUT);

		if(this->bl)
			this->bl->Mode(EMode::OUTPUT);

		Commit();

		memset(dram_state, 0, sizeof(dram_state));
		memset(dram_new, TranslateCharToDram(' '), sizeof(dram_new));

		SetRaw(false, 0b00111000);	// 8bit interface
		TFiber::Sleep(0.0004100);
		SetRaw(false, 0b00111000);	// 8bit interface
		TFiber::Sleep(0.0000100);
		SetRaw(false, 0b00111000);	// 8bit interface

		SendCommand(0b00001000);	// off
		SendCommand(0b00000001);	// clear
		SendCommand(0b00000010);	// return home
		SendCommand(0b00000110);	// entry mode set; auto increment address, no shift

		EL_ERROR(GetAddress() != 0, TException, "unable to initialize HD44780 LCD - initial DRAM address not zero");

		if(DEBUG)
		{
			SendCommand(0b00001111);	// display on, cursor, blinking

			for(u8_t i = 0; i < 80; i++)
				SetAddress(i);
			SetAddress(0);

			u8_t expected_addr = 0;
			for(u8_t i = 0; i < 80; i++)
			{
				u8_t ret_addr;

				EL_WARN((ret_addr = GetAddress()) != expected_addr, TException, TString::Format("DRAM address suddenly changed to an unexpected value before writing byte %d (expected: %d, received: %d)", i, expected_addr, ret_addr));
				expected_addr = ret_addr;

				SendData('#');
				expected_addr++;

				if(expected_addr == 80)
					expected_addr = 0;

				EL_WARN((ret_addr = GetAddress()) != expected_addr, TException, TString::Format("DRAM address suddenly changed to an unexpected value after writing byte %d (expected: %d, received: %d)", i, expected_addr, ret_addr));
				expected_addr = ret_addr;
			}

			SetAddress(0);

			TFiber::Sleep(5);
		}
		else
		{
			SendCommand(0b00001100);	// display on, no cursor, no blinking
		}

		Render();
	}

	THD44780::~THD44780()
	{
		if(!DEBUG)
		{
			SendCommand(0b00000001);	// clear
			SendCommand(0b00001000);	// off
			Backlight(false);
		}
	}
}
