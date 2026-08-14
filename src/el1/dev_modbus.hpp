#pragma once

/**
 * @file modbus.hpp
 * @brief Minimal Modbus RTU master-side abstraction.
 *
 * General Modbus explanation:
 * - Modbus is a simple request/response protocol between a master and one or more slave devices.
 * - Each slave is addressed by a unit-id (device-id) on the bus (RTU: RS-485 / serial line).
 * - This interface implements the master-side of the protocol.
 * - The protocol exposes four logical data tables (address spaces):
 *
 *   1) Coils (single-bit, read/write)
 *      - Represent discrete outputs (e.g. relay state, enable signals).
 *      - Accessed via function codes:
 *        - 0x01: READ_COILS
 *        - 0x05: WRITE_SINGLE_COIL
 *        - 0x0F: WRITE_MULTIPLE_COILS
 *
 *   2) Discrete Inputs (single-bit, read-only)
 *      - Represent discrete input states (e.g. limit switches, digital sensors).
 *      - Accessed via function code:
 *        - 0x02: READ_DISCRETE_INPUTS
 *
 *   3) Input Registers (16-bit, read-only)
 *      - Represent read-only 16-bit values (e.g. measurements, sensor readings).
 *      - Accessed via function code:
 *        - 0x04: READ_INPUT_REGISTERS
 *
 *   4) Holding Registers (16-bit, read/write)
 *      - Represent 16-bit configuration values, set-points, status words or general parameters.
 *      - Accessed via function codes:
 *        - 0x03: READ_HOLDING_REGISTERS
 *        - 0x06: WRITE_SINGLE_REGISTER
 *        - 0x10: WRITE_MULTIPLE_REGISTERS
 *        - 0x16: MASK_WRITE_REGISTER
 *        - 0x17: READWRITE_MULTIPLE_REGISTERS
 *
 * On the wire, Modbus only knows:
 * - single bits (for coils / discrete inputs),
 * - 16-bit registers (for input / holding registers).
 *
 * All larger data types (32-bit integers, floating point, 64-bit values, structured data)
 * are device-specific conventions built from multiple 16-bit registers and/or bitfields.
 */

#include "io_stream.hpp"
#include "io_collection_list.hpp"
#include "dev_tty.hpp"
#include "dev_gpio.hpp"
#include <bit>
#include <type_traits>

namespace el1::dev::modbus
{
	using namespace io::types;
	using namespace io::collection::list;
	using namespace system::time;

	class TModBus;
	class TDevice;

	/**
	 * @enum EFunctionCode
	 * @brief Standard Modbus function codes used by this interface.
	 *
	 * These values correspond to the Modbus application protocol
	 * function codes for reading/writing coils and registers and for
	 * selected advanced operations.
	 */
	enum class EFunctionCode : u8_t
	{
		READ_COILS                   = 0X01,
		READ_DISCRETE_INPUTS         = 0X02,
		READ_HOLDING_REGISTERS       = 0X03,
		READ_INPUT_REGISTERS         = 0X04,
		WRITE_SINGLE_COIL            = 0X05,
		WRITE_SINGLE_REGISTER        = 0X06,
		WRITE_MULTIPLE_COILS         = 0X0F,
		WRITE_MULTIPLE_REGISTERS     = 0X10,
		MASK_WRITE_REGISTER          = 0X16,
		READWRITE_MULTIPLE_REGISTERS = 0X17,
		ENCAPSULATED_INTERFACE       = 0X2B
	};

	/**
	 * @enum EExceptionCode
	 * @brief Standard Modbus exception codes returned by a slave.
	 *
	 * If a slave cannot process a request, it returns a normal Modbus
	 * response with the function code ORed with 0x80 and one of these
	 * exception codes in the data field.
	 */
	enum class EExceptionCode : u8_t
	{
		NONE                             = 0x00,
		ILLEGAL_FUNCTION                 = 0x01,
		ILLEGAL_DATA_ADDRESS             = 0x02,
		ILLEGAL_DATA_VALUE               = 0x03,
		SLAVE_DEVICE_FAILURE             = 0x04,
		ACKNOWLEDGE                      = 0x05,
		SLAVE_DEVICE_BUSY                = 0x06,
		MEMORY_PARITY_ERROR              = 0x08,
		GATEWAY_PATH_UNAVAILABLE         = 0x0A,
		GATEWAY_TARGET_FAILED_TO_RESPOND = 0x0B
	};

	const char* ExceptionCodeToString(const EExceptionCode code);

	/**
	 * @class TSlaveException
	 * @brief Exception type used to represent Modbus device exceptions.
	 *
	 * This exception encapsulates the function code, exception code,
	 * device-id and register-address context of an error. It is
	 * thrown in when a devive returns an error code to the host.
	 */
	class TSlaveException : public error::IException
	{
		public:
			const EFunctionCode function;
			const EExceptionCode code;
			const u8_t device_id;

			io::text::string::TString Message() const final override;
			IException* Clone() const override;

			TSlaveException(const EFunctionCode function, const EExceptionCode code, const u8_t device_id);
	};

	struct EL_PACKED frame_header_t
	{
		/** Modbus device (slave) address. */
		u8_t device_id;

		/** Modbus function code for this frame. */
		EFunctionCode function;

		/** Base register address (big-endian on the wire). */
		u16_t reg_addr;
	};

	struct EL_PACKED frame_footer_t
	{
		/** CRC16 of the complete frame payload (Modbus RTU polynomial). */
		u16_t crc;
	};

	struct TFrameBuffer
	{
		/** Maximum number of data bytes (payload) that can be stored in the buffer. */
		static const unsigned MAX_ARRAY_SIZE = 255;

		/** Number of bytes reserved for the array length prefix. */
		static const unsigned ARRAY_COUNTER_SIZE = 1;

		/** Maximum size of a request frame (header + length + payload + footer). */
		static const unsigned MAX_REQUEST_SIZE = sizeof(frame_header_t) + ARRAY_COUNTER_SIZE + MAX_ARRAY_SIZE + sizeof(frame_footer_t);

		/** Maximum size of a response frame (device_id + function + length + payload + footer). */
		static const unsigned MAX_RESPONSE_SIZE = 2 + ARRAY_COUNTER_SIZE + MAX_ARRAY_SIZE + sizeof(frame_footer_t);

		/** Maximum of request/response frame sizes; actual buffer size. */
		static const unsigned MAX_FRAME_SIZE = util::Max(MAX_REQUEST_SIZE, MAX_RESPONSE_SIZE);

		/** Write position in the buffer (number of valid bytes). */
		u16_t pos_write;

		/** Read position in the buffer. */
		u16_t pos_read;

		/** Raw frame storage. */
		byte_t buffer[MAX_FRAME_SIZE];

		/**
		* Compare this frame buffer with another instance.
		*
		* Two frame buffers are considered equal if they contain the same number
		* of bytes and all bytes up to pos_write are identical.
		*
		* @param rhs
		*     Other frame buffer to compare against.
		* @return
		*     true if both buffers match, false otherwise.
		*/
		bool operator==(const TFrameBuffer& rhs) const;

		/**
		* Ensure that there is enough remaining space to append n_need bytes.
		*
		* Throws an exception if the requested size would exceed MAX_FRAME_SIZE.
		*
		* @param n_need
		*     Number of bytes that will be written next.
		*/
		void EnsureSpace(const unsigned n_need);

		/**
		* Append a single byte to the frame buffer.
		*
		* @param v
		*     Byte value to write.
		*/
		void WriteByte(const byte_t v);

		/**
		* Append a 16-bit word in big-endian order to the frame buffer.
		*
		* @param v
		*     16-bit word to write.
		*/
		void WriteWord(const u16_t v);

		/**
		* Write the Modbus request header at the beginning of the buffer.
		*
		* The header is written as-is, except that the register address is converted
		* to big-endian byte order.
		*
		* @param h
		*     Frame header to write.
		*/
		void WriteRequestHeader(frame_header_t h);

		/**
		* Append an array of 16-bit words to the frame buffer.
		*
		* The function writes a length prefix in bytes, followed by each value
		* in big-endian order.
		*
		* @param n
		*     Number of array elements.
		* @param arr
		*     Pointer to the first element in the array.
		*/
		void WriteArray(const unsigned n, const u16_t* const arr);

		/**
		* Append an array of boolean coil/discrete values to the frame buffer.
		*
		* Values are packed bitwise into bytes (LSB-first) and preceded by a
		* length prefix in bytes.
		*
		* @param n
		*     Number of boolean elements.
		* @param arr
		*     Pointer to the first boolean element.
		*/
		void WriteArray(const unsigned n, const bool* const arr);

		/**
		* Compute the Modbus RTU CRC16 over a byte array.
		*
		* Polynomial: 0xA001, initial value: 0xFFFF, LSB-first algorithm.
		*
		* @param n
		*     Number of bytes to include in the CRC.
		* @param arr
		*     Pointer to the first byte.
		* @return
		*     CRC16 value.
		*/
		static u16_t ComputeCRC(const unsigned n, const byte_t* const arr);

		/**
		* Append a CRC16 over the current buffer contents.
		*
		* The CRC is computed over all bytes up to pos_write and appended in
		* Modbus RTU wire order: low byte first, then high byte.
		*/
		void WriteCRC();

		/**
		* Validate the CRC of the current frame.
		*
		* Recomputes the CRC over all bytes except the last two and compares it
		* against the stored CRC at the end of the buffer.
		*
		* Throws an exception on mismatch or if the frame is too short.
		*/
		void VerifyCRC();

		/**
		* Ensure that reading n_want bytes from the current read position will
		* not exceed the size of the current frame.
		*
		* @param n_want
		*     Number of bytes the caller intends to read.
		*/
		void EnsureRemaining(const unsigned n_want);

		/**
		* Read and return a single byte from the buffer.
		*
		* Advances the read position by one.
		*
		* @return
		*     The byte read from the buffer.
		*/
		byte_t ReadByte();

		/**
		* Read and return a 16-bit word in host native order from the buffer.
		*
		* Advances the read position by two.
		*
		* @return
		*     The 16-bit word read from the buffer.
		*/
		u16_t ReadWord();

		/**
		* Read a single byte and verify that it matches the expected value.
		*
		* @param field_name
		*     Human-readable field name used for error reporting.
		* @param expected_value
		*     Expected byte value.
		*
		* Throws an exception if the actual value differs.
		*/
		void ReadByteExpected(const char* const field_name, const byte_t expected_value);

		/**
		* Read a 16-bit word and verify that it matches the expected value.
		*
		* @param field_name
		*     Human-readable field name used for error reporting.
		* @param expected_value
		*     Expected 16-bit value.
		*
		* Throws an exception if the actual value differs.
		*/
		void ReadWordExpected(const char* const field_name, const u16_t expected_value);

		/**
		* Read a packed boolean array from the buffer.
		*
		* Expects a length byte (in bytes) followed by packed bits representing
		* n_expected boolean values (LSB-first).
		*
		* @param n_expected
		*     Number of boolean values that are expected.
		* @param arr
		*     Output array for the unpacked boolean values.
		*/
		void ReadArray(const u16_t n_expected, bool* const arr);

		/**
		* Read an array of 16-bit words from the buffer.
		*
		* Expects a length byte in bytes followed by n_expected 16-bit values
		* in big-endian order.
		*
		* @param n_expected
		*     Number of 16-bit elements that are expected.
		* @param arr
		*     Output array for the read values.
		*/
		void ReadArray(const u16_t n_expected, u16_t* const arr);

		/**
		* Transmit the current frame over the given teletypewriter.
		*
		* Optionally verifies line integrity by reading back the transmitted
		* bytes (loopback) and comparing them to the buffer contents.
		*
		* @param tty
		*     Teletypewriter used for transmission.
		* @param tx_echo
		*     If true, perform a loopback read and verify the transmitted data.
		*/
		void Send(tty::TTeletypewriter& tty, const bool tx_echo, TTime& ts_eof) const;

		/**
		* Receive an exact number of bytes from the teletypewriter.
		*
		* Blocks until either n_bytes have been read or a timeout/short read
		* condition occurs.
		*
		* @param tty
		*     Teletypewriter to read from.
		* @param n_bytes
		*     Number of bytes expected.
		*/
		void ReceiveExcact(tty::TTeletypewriter& tty, const u16_t n_bytes);

		/**
		* Get the number of remaining free bytes in the buffer.
		*
		* @return
		*     Maximum additional bytes that can be appended.
		*/
		u16_t Space() const;

		/**
		* Receive a complete Modbus response frame.
		*
		* This function:
		* - Waits for the first byte of the response within response_timeout.
		* - Continues reading until a Modbus RTU inter-frame timeout (t3.5) occurs.
		* - Verifies the CRC.
		* - Checks device ID and function code.
		* - Raises an exception on Modbus exception response.
		*
		* @param tty
		*     Teletypewriter to read from.
		* @param response_timeout
		*     Maximum time to wait for the first response byte.
		* @param device_id
		*     Expected slave address.
		* @param function
		*     Expected function code.
		*/
		void ReceiveResponse(tty::TTeletypewriter& tty, const TTime response_timeout, const u8_t device_id, EFunctionCode function, TTime& ts_eof);

		// sleep until the frame gap has passed
		void SleepFrameGap(tty::TTeletypewriter& tty, TTime& ts_eof) const;

		/**
		* Construct an empty frame buffer with reset read/write positions.
		*/
		TFrameBuffer();
	};

	/**
	 * @class TDevice
	 * @brief Logical Modbus slave (device) on a TModBus.
	 *
	 * TDevice represents a single slave device identified by a device-id
	 * (unit-id) on the shared bus. It provides typed accessors for coils,
	 * discrete inputs, holding registers and input registers.
	 */
	class TDevice
	{
		public:
			TModBus* const bus;
			const u8_t id;

			constexpr TDevice(TModBus* const bus, const u8_t id) : bus(bus), id(id) {}

			bool ReadCoil(const u16_t address) const;
			void ReadCoils(const u16_t start_address, const u16_t n_coils, bool* const arr_state) const;

			void WriteCoil(const u16_t address, const bool new_state);
			void WriteCoils(const u16_t start_address, const u16_t n_coils, const bool* const arr_state);

			bool ReadDiscreteInput(const u16_t address) const;
			void ReadDiscreteInputs(const u16_t start_address, const u16_t n_registers, bool* const arr_state) const;

			u16_t ReadHoldingRegister(const u16_t address) const;
			void ReadHoldingRegisters(const u16_t start_address, const u16_t n_registers, u16_t* const arr_values) const;

			void WriteHoldingRegister(const u16_t address, const u16_t new_value);
			void WriteHoldingRegisters(const u16_t start_address, const u16_t n_registers, const u16_t* const arr_values);

			u16_t ReadInputRegister(const u16_t address) const;
			void ReadInputRegisters(const u16_t start_address, const u16_t n_registers, u16_t* const arr_values) const;
	};

	/**
	 * @class TModBus
	 * @brief Modbus RTU master using abstract ISource/ISink byte streams.
	 *
	 * TModBus implements the master-side Modbus RTU framing and request/
	 * response handling on top of a TTeletypewriter interface.
	 *
	 * It provides primitive operations for reading and writing individual
	 * coils and registers of a given device-id. Higher-level abstractions
	 * are offered by TDevice and the register wrapper classes.
	 */
	class TModBus
	{
		protected:
			tty::TTeletypewriter* const tty;
			const TTime response_timeout;
			const bool tx_echo;
			mutable TTime ts_eof;

		public:
			bool lax_response;
			static bool DEBUG;
			/**
			 * Constructs a Modbus RTU master on a serial link.
			 *
			 * This class assumes it is the only master on the bus and that the bus
			 * is exclusively used for modbus data and not shared with any other
			 * protocol.
			 *
			 * @param tty RS-485 interface
			 * @param response_timeout
			 *        Maximum time to wait for a slave to *begin* sending a
			 *        response after we sent a request. Essentially how long
			 *        the link can become idle after a request has been sent.
			 * @param tx_echo
			 *        If true, the receive stream will see a copy of all
			 *        bytes written to the transmit stream (local echo on
			 *        the link). In this case we immediately readback our own
			 *        TX and verify it matches with what we intended to send
			 *        and then discard it.
			 *        If false, only slave responses are expected on the receive stream.
			 * @param lax_response If set, some checks on slave responses are relaxed.
			 */
			TModBus(
				tty::TTeletypewriter* const tty,
				const TTime response_timeout,
				const bool tx_echo
			);

			std::unique_ptr<TDevice> ClaimDevice(const u8_t device_id);

			bool ReadCoil(const u8_t device_id, const u16_t address) const;
			void ReadCoils(const u8_t device_id, const u16_t start_address, const u16_t n_coils, bool* const arr_state) const;

			void WriteCoil(const u8_t device_id, const u16_t address, const bool new_state);
			void WriteCoils(const u8_t device_id, const u16_t start_address, const u16_t n_coils, const bool* const arr_state);

			bool ReadDiscreteInput(const u8_t device_id, const u16_t address) const;
			void ReadDiscreteInputs(const u8_t device_id, const u16_t start_address, const u16_t n_registers, bool* const arr_state) const;

			u16_t ReadHoldingRegister(const u8_t device_id, const u16_t address) const;
			void ReadHoldingRegisters(const u8_t device_id, const u16_t start_address, const u16_t n_registers, u16_t* const arr_values) const;

			void WriteHoldingRegister(const u8_t device_id, const u16_t address, const u16_t new_value);
			void WriteHoldingRegisters(const u8_t device_id, const u16_t start_address, const u16_t n_registers, const u16_t* const arr_values);

			u16_t ReadInputRegister(const u8_t device_id, const u16_t address) const;
			void ReadInputRegisters(const u8_t device_id, const u16_t start_address, const u16_t n_registers, u16_t* const arr_values) const;
	};

	template<typename T, u16_t n_reg, bool msr_first = false, int shift = 0>
	class TMRegType
	{
		protected:
			static_assert(n_reg > 0 && n_reg <= 4, "TMRegType: n_reg must be in [1, 4]");
			static_assert(std::is_trivially_copyable_v<T>, "TMRegType: T must be trivially copyable");
			static_assert(sizeof(T) <= sizeof(u64_t), "TMRegType: T must fit into 64 bits");
			static_assert(sizeof(T) <= n_reg * sizeof(u16_t), "TMRegType: T does not fit into the selected register count");
			static_assert(shift > -64 && shift < 64, "TMRegType: shift must be in (-64, 64)");

			static u64_t ToRawBits(const T value)
			{
				if constexpr(sizeof(T) == sizeof(u8_t))
					return std::bit_cast<u8_t>(value);
				else if constexpr(sizeof(T) == sizeof(u16_t))
					return std::bit_cast<u16_t>(value);
				else if constexpr(sizeof(T) == sizeof(u32_t))
					return std::bit_cast<u32_t>(value);
				else if constexpr(sizeof(T) == sizeof(u64_t))
					return std::bit_cast<u64_t>(value);
				else
					static_assert(sizeof(T) == 1 || sizeof(T) == 2 || sizeof(T) == 4 || sizeof(T) == 8, "TMRegType: unsupported T size");
			}

			static T FromRawBits(const u64_t raw)
			{
				if constexpr(sizeof(T) == sizeof(u8_t))
					return std::bit_cast<T>((u8_t)raw);
				else if constexpr(sizeof(T) == sizeof(u16_t))
					return std::bit_cast<T>((u16_t)raw);
				else if constexpr(sizeof(T) == sizeof(u32_t))
					return std::bit_cast<T>((u32_t)raw);
				else if constexpr(sizeof(T) == sizeof(u64_t))
					return std::bit_cast<T>((u64_t)raw);
				else
					static_assert(sizeof(T) == 1 || sizeof(T) == 2 || sizeof(T) == 4 || sizeof(T) == 8, "TMRegType: unsupported T size");
			}

			static void EncodeRegisters(u64_t raw, u16_t* const regs)
			{
				for(u16_t i = 0; i < n_reg; i++)
				{
					const u16_t reg = (u16_t)((raw >> (16U * i)) & 0xFFFFU);
					regs[msr_first ? n_reg - 1 - i : i] = reg;
				}
			}

			static u64_t DecodeRegisters(const u16_t* const regs)
			{
				u64_t raw = 0;
				for(u16_t i = 0; i < n_reg; i++)
					raw |= (u64_t)regs[msr_first ? n_reg - 1 - i : i] << (16U * i);
				return raw;
			}

		public:
			TDevice* const device;
			const u16_t addr_base;

			constexpr TMRegType(TDevice* const device, const u16_t addr_base) : device(device), addr_base(addr_base) {}

			TMRegType& operator=(const T new_value)
			{
				u64_t raw = ToRawBits(new_value);
				if constexpr(shift > 0)
					raw <<= shift;
				else if constexpr(shift < 0)
					raw >>= -shift;

				u16_t regs[n_reg];
				EncodeRegisters(raw, regs);
				device->bus->WriteHoldingRegisters(device->id, addr_base, n_reg, regs);
				return *this;
			}

			T operator*() const
			{
				u16_t regs[n_reg];
				device->bus->ReadHoldingRegisters(device->id, addr_base, n_reg, regs);
				u64_t raw = DecodeRegisters(regs);
				if constexpr(shift > 0)
					raw >>= shift;
				else if constexpr(shift < 0)
					raw <<= -shift;
				return FromRawBits(raw);
			}
	};
}
