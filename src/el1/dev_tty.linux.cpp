#include "def.hpp"
#ifdef EL_OS_LINUX

#include "dev_tty.hpp"
#include <sys/ioctl.h>
#include <asm/ioctls.h>   // TCGETS2, TCSETS2
#include <asm/termbits.h>	// struct termios2, CBAUD, BOTHER, etc.
#include <linux/serial.h>	// struct serial_rs485, TIOCSRS485, TIOCGRS485

extern "C" int tcdrain(int fd);

namespace el1::dev::tty
{
	TConfiguration TConfiguration::FromTerminalConfig(const terminal_config_t tc)
	{
		TConfiguration cfg{};

		// data bits
		switch(tc.c_cflag & CSIZE)
		{
			case CS5:
				cfg.data_bits = EDataBits::FIVE;
				break;
			case CS6:
				cfg.data_bits = EDataBits::SIX;
				break;
			case CS7:
				cfg.data_bits = EDataBits::SEVEN;
				break;
			case CS8:
			default:
				cfg.data_bits = EDataBits::EIGHT;
				break;
		}

		// parity
		if(!(tc.c_cflag & PARENB))
		{
			cfg.parity = EParity::NONE;
		}
		else
		{
			if(tc.c_cflag & CMSPAR)
			{
				cfg.parity = (tc.c_cflag & PARODD) ? EParity::MARK : EParity::SPACE;
			}
			else
			{
				cfg.parity = (tc.c_cflag & PARODD) ? EParity::ODD : EParity::EVEN;
			}
		}

		// stop bits
		cfg.stop_bits = (tc.c_cflag & CSTOPB) ? EStopBits::TWO : EStopBits::ONE;

		// software flow control
		cfg.handle_xonoff = (tc.c_iflag & IXON) != 0;
		cfg.send_xonoff = (tc.c_iflag & IXOFF) != 0;

		// hardware flow control (RTS/CTS)
		cfg.rts_cts = (tc.c_cflag & CRTSCTS) != 0;

		// line discipline
		cfg.line_mode = (tc.c_lflag & ICANON) != 0;
		cfg.signals   = (tc.c_lflag & ISIG) != 0;
		cfg.echo      = (tc.c_lflag & ECHO) != 0;

		// modem-style hangup semantics
		cfg.modem = (tc.c_cflag & HUPCL) != 0;

		// RS-485 / hardware enable flags cannot be derived from termios2 alone
		cfg.hardware_tx_en = false;
		cfg.hardware_rx_en = false;

		return cfg;
	}

	const TConfiguration TConfiguration::RAW_BINARY =
		{
			EDataBits::EIGHT,
			EParity::NONE,
			EStopBits::ONE,
			false,	// handle_xonoff
			false,	// send_xonoff
			false,	// rts_cts
			false,	// line_mode
			false,	// signals
			false,	// echo
			false,	// modem
			false,	// hardware_tx_en
			false	// hardware_rx_en
		};

	const TConfiguration TConfiguration::LINE_MODE =
		{
			EDataBits::EIGHT,
			EParity::NONE,
			EStopBits::ONE,
			true,	// handle_xonoff
			false,	// send_xonoff
			false,	// rts_cts
			true,	// line_mode (canonical)
			true,	// signals (Ctrl-C, Ctrl-Z → signals)
			true,	// echo
			false,	// modem
			false,	// hardware_tx_en
			false	// hardware_rx_en
		};

	/////////////////////////////////////////////////////////////////

	static terminal_config_t GetCurrentTerminalConfig(TKernelStream& stream)
	{
		terminal_config_t cfg;
		EL_SYSERR(ioctl(stream.handle, TCGETS2, &cfg));
		return cfg;
	}

	static void SetTerminalConfig(TKernelStream& stream, const TConfiguration cfg, const u32_t current_baudrate)
	{
		const int fd = stream.handle;

		termios2 tio = {};
		EL_SYSERR(ioctl(fd, TCGETS2, &tio));

		tio.c_oflag &= ~(OPOST|ONLCR|OCRNL);
		tio.c_iflag &= ~(IGNBRK|BRKINT|PARMRK|ISTRIP|INLCR|IGNCR|ICRNL|IUCLC);
		tio.c_cflag |= CREAD;

		if(current_baudrate != 0)
		{
			tio.c_cflag &= ~CBAUD;
			tio.c_cflag |= BOTHER;
			tio.c_ispeed = current_baudrate;
			tio.c_ospeed = current_baudrate;
		}

		tio.c_cflag &= ~CSIZE;
		switch(cfg.data_bits)
		{
			case EDataBits::FIVE:
				tio.c_cflag |= CS5;
				break;
			case EDataBits::SIX:
				tio.c_cflag |= CS6;
				break;
			case EDataBits::SEVEN:
				tio.c_cflag |= CS7;
				break;
			case EDataBits::EIGHT:
			default:
				tio.c_cflag |= CS8;
				break;
		}

		tio.c_cflag &= ~(PARENB | PARODD);
		tio.c_cflag &= ~CMSPAR;

		switch(cfg.parity)
		{
			case EParity::NONE:
				tio.c_iflag &= ~(INPCK|IGNPAR);
				break;

			case EParity::EVEN:
				tio.c_cflag |= PARENB;
				tio.c_iflag |= INPCK;
				break;

			case EParity::ODD:
				tio.c_cflag |= PARENB | PARODD;
				tio.c_iflag |= INPCK;
				break;

			case EParity::MARK:
				tio.c_cflag |= PARENB | PARODD | CMSPAR;
				tio.c_iflag |= INPCK;
				break;

			case EParity::SPACE:
				tio.c_cflag |= PARENB | CMSPAR;
				tio.c_iflag |= INPCK;
				break;
		}

		if(cfg.stop_bits == EStopBits::TWO)
			tio.c_cflag |= CSTOPB;
		else
			tio.c_cflag &= ~CSTOPB;

		if(cfg.rts_cts && !cfg.hardware_tx_en)
			tio.c_cflag |= CRTSCTS;
		else
			tio.c_cflag &= ~CRTSCTS;

		if(cfg.modem)
		{
			tio.c_cflag |= HUPCL;
			tio.c_cflag &= ~CLOCAL;
		}
		else
		{
			tio.c_cflag &= ~HUPCL;
			tio.c_cflag |= CLOCAL;
		}

		if(cfg.handle_xonoff)
			tio.c_iflag |= IXON;
		else
			tio.c_iflag &= ~IXON;

		if(cfg.send_xonoff)
			tio.c_iflag |= IXOFF;
		else
			tio.c_iflag &= ~IXOFF;

		if(cfg.line_mode)
		{
			tio.c_lflag |= (ICANON | IEXTEN);
			tio.c_iflag |= ICRNL;
		}
		else
		{
			tio.c_lflag &= ~(ICANON | IEXTEN);
		}

		if(cfg.signals)
			tio.c_lflag |= ISIG;
		else
			tio.c_lflag &= ~ISIG;

		if(cfg.echo)
			tio.c_lflag |= (ECHO|ECHOE|ECHOK|ECHOCTL|ECHOKE);
		else
			tio.c_lflag &= ~(ECHO|ECHOE|ECHOK|ECHOCTL|ECHOKE);

		EL_SYSERR(ioctl(fd, TCSETS2, &tio));

		serial_rs485 rs485 = {};
		const int rs485_result = ioctl(fd, TIOCGRS485, &rs485);
		if(rs485_result == 0)
		{
			if(cfg.hardware_tx_en || cfg.hardware_rx_en)
			{
				rs485.flags |= SER_RS485_ENABLED;

				if(cfg.hardware_tx_en)
				{
					rs485.flags |= SER_RS485_RTS_ON_SEND;
					rs485.flags &= ~SER_RS485_RTS_AFTER_SEND;
				}

				if(cfg.hardware_rx_en)
					rs485.flags &= ~SER_RS485_RX_DURING_TX;
				else
					rs485.flags |= SER_RS485_RX_DURING_TX;
			}
			else
			{
				rs485.flags &= ~SER_RS485_ENABLED;
			}

			EL_SYSERR(ioctl(fd, TIOCSRS485, &rs485));
		}
		else if(cfg.hardware_tx_en || cfg.hardware_rx_en)
		{
			EL_SYSERR(rs485_result);
		}
	}

	void TTeletypewriter::Config(const TConfiguration new_config)
	{
		SetTerminalConfig(*this, new_config, current_baudrate);
		current_config = new_config;
	}

	TConfiguration TTeletypewriter::Config() const
	{
		return current_config;
	}

	void TTeletypewriter::Baudrate(const u32_t new_baudrate)
	{
		EL_ERROR(new_baudrate == 0, TInvalidArgumentException, "new_baudrate", "baud rate must be greater than zero");
		SetTerminalConfig(*this, current_config, new_baudrate);
		current_baudrate = new_baudrate;
	}

	u32_t TTeletypewriter::Baudrate() const
	{
		return current_baudrate;
	}

	void TTeletypewriter::TxSync()
	{
		EL_SYSERR(tcdrain(handle));
	}

	void TTeletypewriter::WriteAll(const byte_t* const arr_items, const usys_t n_items)
	{
		if(rx_en)
			rx_en->State(false);
		if(tx_en)
			tx_en->State(true);

		try
		{
			TKernelStream::WriteAll(arr_items, n_items);
			TxSync();
		}
		catch(...)
		{
			if(tx_en)
				tx_en->State(false);
			if(rx_en)
				rx_en->State(true);
			throw;
		}

		if(tx_en)
			tx_en->State(false);
		if(rx_en)
			rx_en->State(true);
	}

	TTeletypewriter::TTeletypewriter(TKernelStream _stream, std::unique_ptr<gpio::IPin> _rx_en, std::unique_ptr<gpio::IPin> _tx_en, const bool restore) :
		TKernelStream(std::move(_stream)),
		rx_en(std::move(_rx_en)),
		tx_en(std::move(_tx_en)),
		tc_original(GetCurrentTerminalConfig(*this)),
		current_config(TConfiguration::FromTerminalConfig(tc_original)),
		current_baudrate(tc_original.c_ospeed),
		restore(restore)
	{
		if(rx_en)
		{
			rx_en->Mode(gpio::EMode::OUTPUT);
			rx_en->State(true);
		}

		if(tx_en)
		{
			tx_en->Mode(gpio::EMode::OUTPUT);
			tx_en->State(false);
		}
	}

	TTeletypewriter::~TTeletypewriter()
	{
		if(restore)
			(void)ioctl(handle, TCSETS2, &tc_original);
	}
}

#endif
