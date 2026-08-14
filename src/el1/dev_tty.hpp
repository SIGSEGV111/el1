#pragma once

#include "def.hpp"
#include "io_stream.hpp"
#include "dev_gpio.hpp"
#include <unistd.h>

#ifdef EL_OS_LINUX
	#include <asm/termbits.h> // struct termios2, CBAUD, BOTHER, etc.
#else
	#include <termios.h>
#endif

namespace el1::dev::tty
{
	using namespace io::stream;

	#ifdef EL_OS_LINUX
		using terminal_config_t = ::termios2;
	#else
		using terminal_config_t = ::termios;
	#endif

	/**
	 * @brief
	 * Teletypewriter (TTY) abstraction for POSIX-like systems.
	 *
	 * A TTY is the traditional Unix abstraction for interactive terminals and serial lines.
	 * Historically, this meant electromechanical teletypes; today it covers:
	 * - Real serial ports (e.g. /dev/ttyS0, /dev/ttyUSB0)
	 * - Pseudo terminals (PTYs) used by terminal emulators
	 * - Virtual consoles
	 *
	 * The kernel exposes a TTY via a file descriptor and a termios configuration.
	 * Key termios concepts:
	 *
	 * - Framing parameters (c_cflag):
	 *   - Character size (CS5..CS8): number of data bits per character.
	 *   - Parity (PARENB, PARODD, CMSPAR): error detection or extra "9th bit".
	 *   - Stop bits (CSTOPB): 1 or 2 stop bits.
	 *   - Flow control (CRTSCTS): hardware RTS/CTS handshake.
	 *
	 * - Input processing (c_iflag):
	 *   - Software flow control (IXON, IXOFF): XON/XOFF handling.
	 *   - Newline translations (ICRNL, INLCR, IGNCR).
	 *   - Parity error handling (INPCK, IGNPAR, PARMRK).
	 *
	 * - Output processing (c_oflag):
	 *   - Newline mapping (ONLCR) and other legacy output translations.
	 *
	 * - Line discipline / local flags (c_lflag):
	 *   - Canonical (line) mode (ICANON): line buffering and basic line editing.
	 *   - Echo (ECHO): local echo of typed characters.
	 *   - Signals (ISIG): map special characters to signals (Ctrl-C → SIGINT, etc.).
	 *
	 * There are two fundamental operating styles:
	 *
	 * - Canonical (cooked) mode:
	 *   - Kernel buffers and edits input line-by-line.
	 *   - Read calls return complete lines.
	 *   - Special characters can generate signals (SIGINT, SIGTSTP, ...).
	 *   - Suitable for shells and simple line-oriented applications.
	 *
	 * - Non-canonical (raw) mode:
	 *   - Bytes are delivered as they arrive, without line editing.
	 *   - Suitable for binary protocols and interactive full-screen UIs.
	 *
	 * TTeletypewriter provides a small, explicit configuration surface over termios
	 * with presets for:
	 * - RAW_BINARY: binary/protocol use, no transformations.
	 * - LINE_MODE: canonical terminal/shell style input.
	 */

	/**
	 * @brief
	 * Parity is used to detect transmission errors.
	 * Parity is computed over the data bits only (5/6/7/8 bits, depending on EDataBits),
	 * not including start, stop, or the parity bit itself.
	 *
	 * NONE  - no parity bit.
	 * EVEN  - The parity bit is chosen so that the total number of 1-bits (data bits + parity bit) is even.
	 * ODD   - The parity bit is chosen so that the total number of 1-bits (data bits + parity bit) is odd.
	 * MARK  - "stick" parity, parity bit always 1.
	 * SPACE - "stick" parity, parity bit always 0.
	 */
	enum class EParity : u8_t
	{
		NONE,
		EVEN,
		ODD,
		MARK,
		SPACE
	};

	/**
	 * @brief
	 * Number of stop bits used in the serial frame.
	 *
	 * ONE - 1 stop bit (default, common).
	 * TWO - 2 stop bits (increased spacing between characters).
	 */
	enum class EStopBits : u8_t
	{
		ONE = 1,
		TWO = 2
	};

	/**
	 * @brief
	 * Number of data bits per character in the serial frame.
	 *
	 * FIVE  - 5 data bits (historical Baudot-style use).
	 * SIX   - 6 data bits (rarely used today).
	 * SEVEN - 7 data bits (pure ASCII).
	 * EIGHT - 8 data bits (modern binary-safe framing).
	 */
	enum class EDataBits : u8_t
	{
		FIVE  = 5,
		SIX   = 6,
		SEVEN = 7,
		EIGHT = 8
	};

	/**
	 * @brief
	 * High-level TTY configuration used by TTeletypewriter.
	 *
	 * This structure describes the logical framing and behavior of the terminal.
	 * It is translated to the underlying termios flags by TTeletypewriter::Config().
	 *
	 * The flags are designed to be explicit and orthogonal:
	 * - data_bits, parity, stop_bits: hardware framing.
	 * - handle_xonoff / send_xonoff / rts_cts: flow control.
	 * - line_mode / signals / echo: line discipline behavior.
	 * - modem: modem control line handling and hangup semantics.
	 */
	struct TConfiguration
	{
		static TConfiguration FromTerminalConfig(const terminal_config_t);

		/**
		 * @brief
		 * Number of data bits per character.
		 */
		EDataBits data_bits;

		/**
		 * @brief
		 * Parity configuration (none/even/odd/mark/space).
		 */
		EParity parity;

		/**
		 * @brief
		 * Number of stop bits (one or two).
		 */
		EStopBits stop_bits;

		/**
		 * @brief
		 * Enable handling of inbound XON/XOFF software flow control.
		 *
		 * When true:
		 * - Received 0x13 (XOFF, Ctrl-S) will pause our transmitter.
		 * - Received 0x11 (XON, Ctrl-Q) will resume our transmitter.
		 * - These characters are consumed by the kernel and will not appear in read() data.
		 *
		 * When false:
		 * - XON/XOFF bytes are delivered to the application as normal data.
		 */
		bool handle_xonoff : 1;

		/**
		 * @brief
		 * Enable automatic outbound XON/XOFF software flow control.
		 *
		 * When true:
		 * - The kernel may transmit 0x13 / 0x11 to the peer when the input queue
		 *   approaches/recovers from overflow, attempting to throttle the peer.
		 * - If the peer does not interpret XON/XOFF, it will see these bytes as normal data.
		 *
		 * When false:
		 * - The kernel never sends XON/XOFF automatically.
		 */
		bool send_xonoff : 1;

		/**
		 * @brief
		 * Enable RTS/CTS hardware flow control where supported.
		 *
		 * When true:
		 * - The driver uses RTS/CTS modem control lines to throttle transmission.
		 * - RTS is asserted/deasserted according to local receive buffer state.
		 * - Transmission is paused while CTS is deasserted.
		 *
		 * When false:
		 * - the driver does not use RTS/CTS for flow control.
		 */
		bool rts_cts : 1;

		/**
		 * @brief
		 * Enable canonical (line) mode.
		 *
		 * When true:
		 * - Input is line-buffered and edited by the kernel.
		 * - read() returns complete lines terminated by EOF/NEWLINE.
		 * - Special erase/kill characters (backspace, Ctrl-U, etc.) are handled.
		 *
		 * When false:
		 * - Input is non-canonical; bytes are delivered as they arrive.
		 */
		bool line_mode : 1;

		/**
		 * @brief
		 * Enable translation of special characters to signals.
		 *
		 * When true:
		 * - The line discipline interprets certain control characters:
		 *   - VINTR (default Ctrl-C)  → SIGINT
		 *   - VSUSP (default Ctrl-Z)  → SIGTSTP
		 *   - VQUIT (default Ctrl-\)  → SIGQUIT
		 * - These characters are consumed and not delivered as data.
		 *
		 * When false:
		 * - These control characters are delivered as normal bytes and no signals are generated.
		 */
		bool signals : 1;

		/**
		 * @brief
		 * Enable local echo of received characters.
		 *
		 * When true:
		 * - Input characters are echoed to the terminal output by the line discipline.
		 *
		 * When false:
		 * - No automatic echo; the application must explicitly write characters it wants displayed.
		 */
		bool echo : 1;

		/**
		 * @brief
		 * Use modem control signals.
		 *
		 * DTR = Data Terminal Ready (PC/host ready)
		 * DSR = Data Set Ready (modem ready)
		 * DCD = Data Carrier Detect (modem detected carrier signal)
		 * RI  = Ring Indicator (incoming call)
		 *
		 * When true:
		 * - Modem control lines (DTR, DSR, DCD, RI) are managed according to driver semantics.
		 * - HUPCL is enabled so that closing the TTY drops the line (hangup on close).
		 *
		 * When false:
		 * - Modem control lines are ignored.
		 */
		bool modem : 1;

		/**
		 * @brief
		 * Enable hardware-controlled transmitter enable signal (e.g. RS-485 DE).
		 *
		 * When set, the UART driver/hardware asserts a dedicated signal line to enable
		 * the transmitter circuit while data is being sent and deasserts it afterwards.
		 * The actual pin used for this function is defined by the hardware design and
		 * board-specific configuration (often RTS), and cannot be selected here.
		 * This *might* conflict with rts_cts hardware flow control and will certainly
		 * conflict with software tx_en control.
		 *
		 * The timing of this signal is handled in hardware or realtime kernel code,
		 * ensuring assertion and deassertion without userspace scheduling delays.
		 */
		bool hardware_tx_en : 1;

		/**
		 * @brief
		 * Disable the receiver while the UART is transmitting.
		 *
		 * When set, the UART driver/hardware disables the receive path during active
		 * transmission. This is useful for half-duplex bus systems (e.g. RS-485) where
		 * TX is looped back to RX and the local node would otherwise see its own data.
		 *
		 * The actual implementation and control signal are defined by the hardware
		 * design and board-specific configuration and cannot be selected here. The same
		 * limitations apply as for hardware_tx_en: this may conflict with rts_cts
		 * hardware flow control or software rx_en control.
		 */
		bool hardware_rx_en : 1;

		/**
		 * @brief
		 * Predefined configuration for raw binary/protocol use.
		 *
		 * - 8 data bits, no parity, 1 stop bit.
		 * - No canonical mode, no echo, no signals.
		 * - No software flow control (XON/XOFF), no RTS/CTS.
		 * - modem control lines ignored
		 */
		static const TConfiguration RAW_BINARY;

		/**
		 * @brief
		 * Predefined configuration for canonical line-mode terminal use.
		 *
		 * - 8 data bits, no parity, 1 stop bit.
		 * - Canonical mode with echo and signals enabled.
		 * - Software flow control for traditional terminal behavior.
		 * - UTF8 encoding used for line editing
		 * - modem control lines ignored
		 */
		static const TConfiguration LINE_MODE;
	};

	/**
	 * @brief
	 * Binary TTY wrapper around a kernel TTY/PTY/serial stream.
	 *
	 * This class exposes a binary I/O interface (ISink/ISource<byte_t>) over a
	 * kernel-backed TTY stream and provides a configuration surface via TConfiguration.
	 *
	 * Important design points:
	 * - All data is treated as binary; the class does not interpret text or line endings.
	 * - Text-specific concerns (UTF-8, CRLF handling, etc.) are delegated to TTextReader/Writer.
	 * - The constructor does not modify the terminal configuration; Config() must be called
	 *   explicitly to change behavior.
	 * - If requested, the original termios configuration is saved and automatically restored
	 *   in the destructor.
	 */
	class TTeletypewriter : public TKernelStream
	{
		protected:
			std::unique_ptr<gpio::IPin> rx_en;
			std::unique_ptr<gpio::IPin> tx_en;
			const terminal_config_t tc_original;
			TConfiguration current_config;
			u32_t current_baudrate;
			const bool restore;

		public:
			/**
			 * @brief
			 * Apply a new TTY configuration to the underlying stream.
			 */
			void Config(const TConfiguration new_config);

			/**
			 * @brief
			 * Query the current high-level configuration for this TTY.
			 */
			TConfiguration Config() const EL_GETTER;

			/**
			 * @brief
			 * Set the baud rate of the TTY.
			 *
			 * This sets both input and output speeds to the given value.
			 *
			 * @param baud Numeric baud rate (e.g. 9600, 115200).
			 */
			void Baudrate(const u32_t new_baudrate);

			/**
			 * @brief
			 * Get the current baud rate of the TTY.
			 *
			 * @return Current baud rate in bits per second.
			 */
			u32_t Baudrate() const EL_GETTER;

			/**
			 * @brief
			 * Block until all pending output has been transmitted.
			 *
			 * This call waits until all data previously written to the TTY has left
			 * the kernel transmit buffer and the UART hardware (i.e. the driver
			 * reports the transmit queue as empty and, where supported, the hardware
			 * FIFO/shift register as idle).
			 *
			 * This is typically used before changing line settings, toggling
			 * RS-485 direction control, or closing the port when it is important
			 * that all queued bytes have actually been sent on the wire.
			 */
			void TxSync();

			/**
			 * @brief
			 * Write a complete block while coordinating optional software RX/TX enable GPIOs.
			 *
			 * When tx_en/rx_en GPIOs were supplied to the constructor, this method disables
			 * the receiver, enables the transmitter, writes the complete block, waits until
			 * the UART has physically drained, and then restores receive mode.
			 */
			void WriteAll(const byte_t* const arr_items, const usys_t n_items) final override;

			/**
			 * @brief
			 * Construct a TTeletypewriter from an existing kernel stream.
			 *
			 * The constructor does not modify the current terminal configuration.
			 * If restore is true, a snapshot of the current configuration is taken
			 * and later restored in the destructor.
			 *
			 * rx_en, if provided, is kept active while WriteAll() is not transmitting.
			 * tx_en, if provided, is enabled for WriteAll() until the UART has drained.
			 * All of this is done in software. So task scheduling delays can have
			 * a serious impact here - especially in half-duplex bus systems like RS-485.
			 * If you plan to use these GPIO signals ensure high task priority and/or
			 * realtime scheduling. If your hardware/driver supports hardware_tx_en
			 * and/or hardware_rx_en, it is recommended to use that instead of software control.
			 *
			 * @param stream Kernel stream that wraps the TTY file descriptor.
			 * @param rx_en  GPIO pin to enable the receiver circuit (optional)
			 * @param tx_en  GPIO pin to enable the transmitter circuit (optional)
			 * @param restore
			 *        If true, capture the current configuration and restore it in the
			 *        destructor; if false, leave configuration untouched on destruction.
			 */
			TTeletypewriter(
				TKernelStream stream,
				std::unique_ptr<gpio::IPin> rx_en = nullptr,
				std::unique_ptr<gpio::IPin> tx_en = nullptr,
				const bool restore = true
			);

			~TTeletypewriter();
	};
}
