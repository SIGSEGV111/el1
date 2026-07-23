#include "dev_spi_si446x.hpp"
#include "error.hpp"
#include "io_text_string.hpp"
#include "system_task.hpp"
#include <string.h>

namespace el1::dev::spi::si446x
{
	using namespace error;
	using namespace io::text::string;
	using namespace system::task;
	using namespace system::time;

	static const TTime T_SHUTDOWN_PULSE = 0.000020;
	static const TTime T_POWER_ON_RESET = 0.014;
	static const TTime T_CTS_POLL_INTERVAL = 0.000050;

	bool TRadio::ReadCommandResponse(byte_t* const response, const usys_t n_response_bytes, const TTime timeout)
	{
		EL_ERROR(n_response_bytes > N_RESPONSE_BYTES_MAX, TInvalidArgumentException, "n_response_bytes", "response is larger than the supported Si446x command response size");

		const TTime deadline = TTime::Now(EClock::MONOTONIC) + timeout;
		byte_t tx[N_RESPONSE_BYTES_MAX + 2] = {};
		byte_t rx[N_RESPONSE_BYTES_MAX + 2] = {};
		tx[0] = (byte_t)ECommand::READ_CMD_BUFF;

		for(;;)
		{
			memset(rx, 0, sizeof(rx));
			device->ExchangeBuffers(tx, rx, n_response_bytes + 2);
			if(rx[1] == 0xff)
			{
				if(response != nullptr && n_response_bytes != 0)
					memcpy(response, rx + 2, n_response_bytes);
				return true;
			}

			if(TTime::Now(EClock::MONOTONIC) >= deadline)
				return false;

			TFiber::Sleep(T_CTS_POLL_INTERVAL);
		}
	}

	void TRadio::Shutdown(const bool new_shutdown)
	{
		if(new_shutdown == is_shutdown)
			return;

		shutdown->State(new_shutdown);
		is_shutdown = new_shutdown;

		if(new_shutdown)
		{
			poll_timer.Stop();
		}
		else
		{
			TFiber::Sleep(T_POWER_ON_RESET);
			if(irq != nullptr)
				irq->AcknowledgeInputTrigger();
			else
				poll_timer.Start(poll_interval);
		}
	}

	void TRadio::Reset()
	{
		if(!is_shutdown)
		{
			shutdown->State(true);
			is_shutdown = true;
			poll_timer.Stop();
		}

		TFiber::Sleep(T_SHUTDOWN_PULSE);
		Shutdown(false);
	}

	bool TRadio::CommandReady()
	{
		EL_ERROR(is_shutdown, TLogicException);
		byte_t response = 0;
		return ReadCommandResponse(&response, 0, 0.0);
	}

	void TRadio::WaitCommandReady(const TTime timeout)
	{
		EL_ERROR(is_shutdown, TLogicException);
		const TTime effective_timeout = timeout < 0 ? command_timeout : timeout;
		EL_ERROR(effective_timeout < 0, TInvalidArgumentException, "timeout", "timeout must not be negative");
		EL_ERROR(!ReadCommandResponse(nullptr, 0, effective_timeout), TException, TString("timeout while waiting for Si446x CTS"));
	}

	void TRadio::SendCommand(const array_t<const byte_t> command)
	{
		EL_ERROR(command.Count() == 0 || command.Count() > N_COMMAND_BYTES_MAX, TInvalidArgumentException, "command", "Si446x command must contain between 1 and 16 bytes");
		WaitCommandReady();
		device->ExchangeBuffers(command.ItemPtr(0), nullptr, command.Count());
	}

	void TRadio::SendCommandGetResponse(const array_t<const byte_t> command, byte_t* const response, const usys_t n_response_bytes)
	{
		EL_ERROR(response == nullptr && n_response_bytes != 0, TInvalidArgumentException, "response", "response buffer is null");
		SendCommand(command);
		EL_ERROR(!ReadCommandResponse(response, n_response_bytes, command_timeout), TException, TString("timeout while waiting for Si446x command response"));
	}

	void TRadio::LoadConfiguration(const TConfiguration& configuration)
	{
		io::collection::list::TArraySource<byte_t> source(configuration.data);
		LoadConfiguration(source);
	}

	void TRadio::LoadConfiguration(io::stream::ISource<byte_t>& source)
	{
		byte_t n_command_bytes = 0;
		for(;;)
		{
			const usys_t n = source.BlockingRead(&n_command_bytes, 1);
			if(n == 0 || n_command_bytes == 0)
				break;

			EL_ERROR(n_command_bytes > N_COMMAND_BYTES_MAX, TException, TString::Format("invalid Si446x configuration command length: %u", (unsigned)n_command_bytes));
			byte_t command[N_COMMAND_BYTES_MAX];
			source.ReadAll(command, n_command_bytes);
			SendCommand(array_t<const byte_t>(command, n_command_bytes));
		}

		WaitCommandReady();
	}

	void TRadio::Initialize(const TConfiguration& configuration)
	{
		Reset();
		LoadConfiguration(configuration);
	}

	void TRadio::Initialize(io::stream::ISource<byte_t>& source)
	{
		Reset();
		LoadConfiguration(source);
	}

	void TRadio::PowerUp(const u32_t xo_frequency_hz, const byte_t boot_options, const byte_t xtal_options)
	{
		byte_t command[7] = {
			(byte_t)ECommand::POWER_UP,
			boot_options,
			xtal_options,
			(byte_t)(xo_frequency_hz >> 24),
			(byte_t)(xo_frequency_hz >> 16),
			(byte_t)(xo_frequency_hz >> 8),
			(byte_t)xo_frequency_hz
		};
		SendCommand(array_t<const byte_t>(command, sizeof(command)));
		WaitCommandReady();
	}

	TPartInfo TRadio::PartInfo()
	{
		const byte_t command[] = { (byte_t)ECommand::PART_INFO };
		byte_t response[8];
		SendCommandGetResponse(array_t<const byte_t>(command, sizeof(command)), response, sizeof(response));
		return {
			.chip_revision = response[0],
			.part = (u16_t)(((u16_t)response[1] << 8) | response[2]),
			.part_build = response[3],
			.id = (u16_t)(((u16_t)response[4] << 8) | response[5]),
			.customer = response[6],
			.rom_id = response[7]
		};
	}

	TDeviceState TRadio::DeviceState()
	{
		const byte_t command[] = { (byte_t)ECommand::REQUEST_DEVICE_STATE };
		byte_t response[2];
		SendCommandGetResponse(array_t<const byte_t>(command, sizeof(command)), response, sizeof(response));
		return { .state = response[0], .channel = response[1] };
	}

	void TRadio::ChangeState(const byte_t state)
	{
		const byte_t command[] = { (byte_t)ECommand::CHANGE_STATE, state };
		SendCommand(array_t<const byte_t>(command, sizeof(command)));
	}

	void TRadio::SetProperties(const byte_t group, const byte_t start_property, const array_t<const byte_t> values)
	{
		EL_ERROR(values.Count() > 256U - (usys_t)start_property, TInvalidArgumentException, "values", "property range exceeds group boundary");

		usys_t offset = 0;
		while(offset < values.Count())
		{
			const usys_t n = util::Min<usys_t>(12, values.Count() - offset);
			byte_t command[N_COMMAND_BYTES_MAX] = {
				(byte_t)ECommand::SET_PROPERTY,
				group,
				(byte_t)n,
				(byte_t)(start_property + offset)
			};
			memcpy(command + 4, values.ItemPtr(offset), n);
			SendCommand(array_t<const byte_t>(command, n + 4));
			offset += n;
		}
		WaitCommandReady();
	}

	void TRadio::GetProperties(const byte_t group, const byte_t start_property, array_t<byte_t> values)
	{
		EL_ERROR(values.Count() > 256U - (usys_t)start_property, TInvalidArgumentException, "values", "property range exceeds group boundary");

		usys_t offset = 0;
		while(offset < values.Count())
		{
			const usys_t n = util::Min<usys_t>(N_RESPONSE_BYTES_MAX, values.Count() - offset);
			const byte_t command[] = {
				(byte_t)ECommand::GET_PROPERTY,
				group,
				(byte_t)n,
				(byte_t)(start_property + offset)
			};
			SendCommandGetResponse(array_t<const byte_t>(command, sizeof(command)), values.ItemPtr(offset), n);
			offset += n;
		}
	}

	TFifoInfo TRadio::FifoInfo(const byte_t reset_flags)
	{
		const byte_t command[] = { (byte_t)ECommand::FIFO_INFO, reset_flags };
		byte_t response[2];
		SendCommandGetResponse(array_t<const byte_t>(command, sizeof(command)), response, sizeof(response));
		return { .rx_count = response[0], .tx_space = response[1] };
	}

	void TRadio::ReadRxFifo(array_t<byte_t> data)
	{
		if(data.Count() == 0)
			return;

		WaitCommandReady();
		std::unique_ptr<byte_t[]> tx(new byte_t[data.Count() + 1]());
		std::unique_ptr<byte_t[]> rx(new byte_t[data.Count() + 1]());
		tx[0] = (byte_t)ECommand::READ_RX_FIFO;
		device->ExchangeBuffers(tx.get(), rx.get(), data.Count() + 1);
		memcpy(data.ItemPtr(0), rx.get() + 1, data.Count());
	}

	void TRadio::WriteTxFifo(const array_t<const byte_t> data)
	{
		if(data.Count() == 0)
			return;

		WaitCommandReady();
		std::unique_ptr<byte_t[]> tx(new byte_t[data.Count() + 1]);
		tx[0] = (byte_t)ECommand::WRITE_TX_FIFO;
		memcpy(tx.get() + 1, data.ItemPtr(0), data.Count());
		device->ExchangeBuffers(tx.get(), nullptr, data.Count() + 1);
	}

	void TRadio::StartTx(const byte_t channel, const u16_t length, const byte_t condition)
	{
		const byte_t command[] = {
			(byte_t)ECommand::START_TX,
			channel,
			condition,
			(byte_t)(length >> 8),
			(byte_t)length
		};
		SendCommand(array_t<const byte_t>(command, sizeof(command)));
	}

	void TRadio::StartRx(const byte_t channel, const u16_t length, const byte_t condition, const byte_t next_state_on_timeout, const byte_t next_state_on_valid_packet, const byte_t next_state_on_invalid_packet)
	{
		const byte_t command[] = {
			(byte_t)ECommand::START_RX,
			channel,
			condition,
			(byte_t)(length >> 8),
			(byte_t)length,
			next_state_on_timeout,
			next_state_on_valid_packet,
			next_state_on_invalid_packet
		};
		SendCommand(array_t<const byte_t>(command, sizeof(command)));
	}

	TInterruptStatus TRadio::InterruptStatus(const byte_t packet_handler_clear_mask, const byte_t modem_clear_mask, const byte_t chip_clear_mask)
	{
		const byte_t command[] = {
			(byte_t)ECommand::GET_INT_STATUS,
			packet_handler_clear_mask,
			modem_clear_mask,
			chip_clear_mask
		};
		byte_t response[8];
		SendCommandGetResponse(array_t<const byte_t>(command, sizeof(command)), response, sizeof(response));
		return {
			.interrupt_pending = response[0],
			.interrupt_status = response[1],
			.packet_handler_pending = response[2],
			.packet_handler_status = response[3],
			.modem_pending = response[4],
			.modem_status = response[5],
			.chip_pending = response[6],
			.chip_status = response[7]
		};
	}

	usys_t TRadio::Read(TInterruptStatus* const events, const usys_t n_events_max)
	{
		if(n_events_max == 0)
			return 0;

		if(irq != nullptr)
		{
			if(irq->State())
			{
				irq->AcknowledgeInputTrigger();
				return 0;
			}
		}
		else
		{
			if(!poll_timer.OnTick().IsReady())
				return 0;
			poll_timer.ReadMissedTicksCount();
		}

		const TInterruptStatus status = InterruptStatus();
		if(irq != nullptr)
			irq->AcknowledgeInputTrigger();

		if(!status.AnyPending())
			return 0;

		events[0] = status;
		return 1;
	}

	const system::waitable::IWaitable* TRadio::OnInputReady() const
	{
		if(irq != nullptr)
			return &irq->OnInputTrigger();
		return &poll_timer.OnTick();
	}

	TRadio::TRadio(std::unique_ptr<ISpiDevice> device, std::unique_ptr<gpio::IPin> shutdown, std::unique_ptr<gpio::IPin> irq, const u32_t hz_clock, const TTime command_timeout, const TTime poll_interval) :
		device(std::move(device)), shutdown(std::move(shutdown)), irq(std::move(irq)), poll_timer(EClock::MONOTONIC), command_timeout(command_timeout), poll_interval(poll_interval), is_shutdown(false)
	{
		EL_ERROR(this->device == nullptr, TInvalidArgumentException, "device", "SPI device is required");
		EL_ERROR(this->shutdown == nullptr, TInvalidArgumentException, "shutdown", "SDN GPIO pin is required");
		EL_ERROR(hz_clock == 0 || hz_clock > HZ_CLOCK_SPI_MAX, TInvalidArgumentException, "hz_clock", "SPI clock must be between 1 Hz and 10 MHz");
		EL_ERROR(command_timeout <= 0, TInvalidArgumentException, "command_timeout", "command timeout must be positive");
		EL_ERROR(poll_interval <= 0, TInvalidArgumentException, "poll_interval", "poll interval must be positive");

		const u64_t effective_clock = this->device->Clock(hz_clock);
		EL_ERROR(effective_clock == 0 || effective_clock > HZ_CLOCK_SPI_MAX, TException, TString::Format("SPI bus configured unsupported Si446x clock frequency: %llu Hz", (unsigned long long)effective_clock));

		this->shutdown->Mode(gpio::EMode::OUTPUT);
		this->shutdown->State(false);

		if(this->irq != nullptr)
		{
			this->irq->Mode(gpio::EMode::INPUT);
			this->irq->Trigger(gpio::ETrigger::FALLING_EDGE);
			this->irq->AcknowledgeInputTrigger();
		}

		Reset();
	}
}
