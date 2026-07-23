#include <gtest/gtest.h>
#include <el1/dev_spi_si446x.hpp>
#include <vector>
#include <string.h>

using namespace ::testing;

namespace
{
	using namespace el1;
	using namespace el1::dev;
	using namespace el1::dev::spi;
	using namespace el1::dev::spi::si446x;
	using namespace el1::io::types;
	using namespace el1::system::waitable;

	class TTestWaitable : public IWaitable
	{
		public:
			bool ready = false;
			bool IsReady() const final override { return ready; }
			void Reset() const final override { const_cast<TTestWaitable*>(this)->ready = false; }
	};

	class TTestPin : public gpio::IPin
	{
		public:
			bool state = false;
			gpio::EMode mode = gpio::EMode::INPUT;
			gpio::ETrigger trigger = gpio::ETrigger::DISABLED;
			gpio::EPull pull = gpio::EPull::DISABLED;
			TTestWaitable waitable;
			usys_t n_acknowledged = 0;

			gpio::IController* Controller() const final override { return nullptr; }
			void AutoCommit(const bool) final override {}
			bool State() const final override { return state; }
			void State(const bool value) final override { state = value; }
			gpio::EMode Mode() const final override { return mode; }
			void Mode(const gpio::EMode value) final override { mode = value; }
			void AlternateFunction(const int) final override {}
			int AlternateFunction() const final override { return 0; }
			gpio::ETrigger Trigger() const final override { return trigger; }
			void Trigger(const gpio::ETrigger value) final override { trigger = value; }
			gpio::EPull Pull() const final override { return pull; }
			void Pull(const gpio::EPull value) final override { pull = value; }
			IWaitable& OnInputTrigger() final override { return waitable; }
			void AcknowledgeInputTrigger() final override { n_acknowledged++; waitable.Reset(); }
			usys_t Index() const final override { return 0; }
	};

	class TTestSpiDevice : public ISpiDevice
	{
		public:
			std::vector<std::vector<byte_t>> commands;
			std::vector<byte_t> last_command;
			u64_t clock = 0;

			ISpiBus* Bus() const final override { return nullptr; }
			void ForceChipEnable(const bool) final override {}
			u64_t Clock(const u64_t value) final override { return clock = value; }

			void ExchangeBuffers(const void* const tx_buffer, void* const rx_buffer, const usys_t n_bytes, const bool) final override
			{
				const byte_t* const tx = (const byte_t*)tx_buffer;
				byte_t* const rx = (byte_t*)rx_buffer;
				if(rx != nullptr)
					memset(rx, 0, n_bytes);

				if(tx == nullptr || n_bytes == 0)
					return;

				if(tx[0] == (byte_t)ECommand::READ_CMD_BUFF)
				{
					if(rx != nullptr && n_bytes >= 2)
						rx[1] = 0xff;
					if(rx == nullptr || n_bytes <= 2 || last_command.empty())
						return;

					switch((ECommand)last_command[0])
					{
						case ECommand::PART_INFO:
						{
							const byte_t response[] = { 0x42, 0x44, 0x63, 0x07, 0x12, 0x34, 0x56, 0x78 };
							memcpy(rx + 2, response, util::Min<usys_t>(sizeof(response), n_bytes - 2));
							break;
						}

						case ECommand::GET_INT_STATUS:
						{
							const byte_t response[] = { 0x01, 0x01, 0x10, 0x10, 0x20, 0x20, 0x40, 0x40 };
							memcpy(rx + 2, response, util::Min<usys_t>(sizeof(response), n_bytes - 2));
							break;
						}

						default:
							break;
					}
					return;
				}

				last_command.assign(tx, tx + n_bytes);
				commands.push_back(last_command);
			}
	};

	TEST(dev_spi_si446x, loads_wds_style_configuration)
	{
		auto* const spi_raw = new TTestSpiDevice();
		auto* const sdn_raw = new TTestPin();
		TRadio radio{std::unique_ptr<ISpiDevice>(spi_raw), std::unique_ptr<gpio::IPin>(sdn_raw)};

		static const byte_t config_data[] = {
			7, (byte_t)ECommand::POWER_UP, 0x01, 0x00, 0x01, 0xc9, 0xc3, 0x80,
			2, (byte_t)ECommand::CHANGE_STATE, 0x03,
			0
		};
		radio.LoadConfiguration(TConfiguration(config_data));

		ASSERT_EQ(spi_raw->commands.size(), 2U);
		EXPECT_EQ(spi_raw->commands[0][0], (byte_t)ECommand::POWER_UP);
		EXPECT_EQ(spi_raw->commands[0].size(), 7U);
		EXPECT_EQ(spi_raw->commands[1][0], (byte_t)ECommand::CHANGE_STATE);
		EXPECT_EQ(spi_raw->commands[1][1], 0x03);
	}

	TEST(dev_spi_si446x, formats_start_tx_command)
	{
		auto* const spi_raw = new TTestSpiDevice();
		TRadio radio{std::unique_ptr<ISpiDevice>(spi_raw), std::unique_ptr<gpio::IPin>(new TTestPin())};

		radio.StartTx(3, 0x1234, 0x80);
		ASSERT_FALSE(spi_raw->commands.empty());
		const std::vector<byte_t>& command = spi_raw->commands.back();
		ASSERT_EQ(command.size(), 5U);
		EXPECT_EQ(command[0], (byte_t)ECommand::START_TX);
		EXPECT_EQ(command[1], 3);
		EXPECT_EQ(command[2], 0x80);
		EXPECT_EQ(command[3], 0x12);
		EXPECT_EQ(command[4], 0x34);
	}

	TEST(dev_spi_si446x, decodes_part_info_response)
	{
		auto* const spi_raw = new TTestSpiDevice();
		TRadio radio{std::unique_ptr<ISpiDevice>(spi_raw), std::unique_ptr<gpio::IPin>(new TTestPin())};

		const TPartInfo info = radio.PartInfo();
		EXPECT_EQ(info.chip_revision, 0x42);
		EXPECT_EQ(info.part, 0x4463);
		EXPECT_EQ(info.part_build, 0x07);
		EXPECT_EQ(info.id, 0x1234);
		EXPECT_EQ(info.customer, 0x56);
		EXPECT_EQ(info.rom_id, 0x78);
	}

	TEST(dev_spi_si446x, exposes_interrupts_as_stream_source)
	{
		auto* const spi_raw = new TTestSpiDevice();
		auto* const irq_raw = new TTestPin();
		irq_raw->state = false;
		irq_raw->waitable.ready = true;
		TRadio radio{std::unique_ptr<ISpiDevice>(spi_raw), std::unique_ptr<gpio::IPin>(new TTestPin()), std::unique_ptr<gpio::IPin>(irq_raw)};
		irq_raw->state = false;
		irq_raw->waitable.ready = true;
		const usys_t acknowledged_before = irq_raw->n_acknowledged;

		TInterruptStatus status = {};
		ASSERT_EQ(radio.Read(&status, 1), 1U);
		EXPECT_EQ(status.interrupt_pending, 0x01);
		EXPECT_EQ(status.packet_handler_pending, 0x10);
		EXPECT_EQ(status.modem_pending, 0x20);
		EXPECT_EQ(status.chip_pending, 0x40);
		EXPECT_EQ(irq_raw->n_acknowledged, acknowledged_before + 1);
		EXPECT_FALSE(irq_raw->waitable.IsReady());
		EXPECT_EQ(radio.OnInputReady(), &irq_raw->waitable);
	}
}
