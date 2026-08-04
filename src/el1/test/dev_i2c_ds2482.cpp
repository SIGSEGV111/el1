#include <gtest/gtest.h>
#include <el1/dev_i2c_ds2482.hpp>
#include <el1/dev_w1_ds18x20.hpp>

using namespace ::testing;

namespace
{
	using namespace el1::dev::i2c;
	using namespace el1::dev::i2c::ds2482;
	using namespace el1::dev::w1;
	using namespace el1::dev::w1::ds18x20;
	using namespace el1::io::collection::list;
	using namespace el1::io::types;

	class TFakeDS2482 final : public II2CDevice
	{
		private:
			u8_t pointer = 0xF0;
			u8_t status = 0x10;
			u8_t configuration = 0;
			u8_t read_data = 0xFF;
			TList<rom_t> roms;
			TList<usys_t> search_candidates;
			TList<u8_t> matched_rom;
			TList<u8_t> read_queue;
			TList<u8_t> scratchpad_writes;
			usys_t scratchpad_write_remaining = 0;
			usys_t search_bit = 0;

			void resetOneWireState()
			{
				matched_rom.Clear();
				read_queue.Clear();
				scratchpad_write_remaining = 0;
				search_bit = 0;
				search_candidates.Clear();
			}

			void queueScratchpad()
			{
				if(roms[0].uuid.type == static_cast<u8_t>(EModel::DS18S20))
				{
					scratchpad_ds18s20_t scratchpad = {};
					scratchpad.temp_raw = -21;
					scratchpad.th = 0xFF;
					scratchpad.tl = 0xFF;
					scratchpad.__reserved1 = 0xFF;
					scratchpad.__reserved2 = 0xFF;
					scratchpad.count_remain = 4;
					scratchpad.count_per_degc = 16;
					scratchpad.crc = CalculateCRC(&scratchpad, sizeof(scratchpad) - 1);
					for(usys_t i = 0; i < sizeof(scratchpad); i++)
						read_queue.Append(reinterpret_cast<const u8_t*>(&scratchpad)[i]);
					return;
				}

				scratchpad_ds18b20_t scratchpad = {};
				scratchpad.temp_raw = static_cast<s16_t>(-10 * 16 - 8);
				scratchpad.th = 0xFF;
				scratchpad.tl = 0xFF;
				scratchpad.config = 0x7F;
				scratchpad.__reserved1 = 0xFF;
				scratchpad.__reserved2 = 0x0C;
				scratchpad.__reserved3 = 0x10;
				scratchpad.crc = CalculateCRC(&scratchpad, sizeof(scratchpad) - 1);
				for(usys_t i = 0; i < sizeof(scratchpad); i++)
					read_queue.Append(reinterpret_cast<const u8_t*>(&scratchpad)[i]);
			}

			void writeOneWireByte(const u8_t value)
			{
				if(scratchpad_write_remaining != 0)
				{
					scratchpad_writes.Append(value);
					scratchpad_write_remaining--;
					return;
				}

				if(matched_rom.Count() == 0 && value == CMD_MATCH_ROM)
				{
					matched_rom.Append(value);
					return;
				}

				if(matched_rom.Count() != 0 && matched_rom.Count() < 9)
				{
					matched_rom.Append(value);
					return;
				}

				switch(value)
				{
					case CMD_POWER_STATE:
						read_queue.Append(0x00);
						break;
					case CMD_WRITE_SCRATCHPAD:
						scratchpad_write_remaining = 3;
						break;
					case CMD_TRIGGER_CONVERSION:
						conversion_used_strong_pullup = (configuration & 0x04) != 0;
						break;
					case CMD_READ_SCRATCHPAD:
						queueScratchpad();
						break;
					case CMD_ENUM_ROM:
						search_bit = 0;
						search_candidates.Clear();
						for(usys_t i = 0; i < roms.Count(); i++)
							search_candidates.Append(i);
						break;
				}
			}

		public:
			bool conversion_used_strong_pullup = false;

			TFakeDS2482(const usys_t n_devices = 1, const EModel model = EModel::DS18B20)
			{
				for(usys_t n = 0; n < n_devices; n++)
				{
					rom_t rom = {};
					rom.uuid.type = static_cast<u8_t>(model);
					for(usys_t i = 0; i < sizeof(rom.uuid.serial.octet); i++)
						rom.uuid.serial.octet[i] = static_cast<u8_t>(i + 1 + n * 17);
					rom.crc = CalculateCRC(&rom.uuid, sizeof(rom.uuid));
					roms.Append(rom);
				}
			}

			const rom_t& Rom(const usys_t index = 0) const
			{
				return roms[index];
			}

			const TList<u8_t>& ScratchpadWrites() const
			{
				return scratchpad_writes;
			}

			usys_t Read(byte_t* const items, const usys_t count) final override
			{
				for(usys_t i = 0; i < count; i++)
				{
					switch(pointer)
					{
						case 0xF0: items[i] = status; break;
						case 0xC3: items[i] = configuration; break;
						case 0xE1: items[i] = read_data; break;
						default: items[i] = 0xFF; break;
					}
				}
				return count;
			}

			usys_t Write(const byte_t* const items, const usys_t count) final override
			{
				if(count == 0)
					return 0;
				switch(items[0])
				{
					case 0xF0:
						status = 0x10;
						configuration = 0;
						pointer = 0xF0;
						resetOneWireState();
						break;
					case 0xE1:
						EXPECT_EQ(count, 2U);
						pointer = items[1];
						break;
					case 0xD2:
						EXPECT_EQ(count, 2U);
						EXPECT_EQ(static_cast<u8_t>((items[1] >> 4) & 0x0F), static_cast<u8_t>((~items[1]) & 0x0F));
						configuration = items[1] & 0x0F;
						pointer = 0xC3;
						break;
					case 0xB4:
						status = 0x02;
						pointer = 0xF0;
						resetOneWireState();
						break;
					case 0xA5:
						EXPECT_EQ(count, 2U);
						writeOneWireByte(items[1]);
						status = 0;
						pointer = 0xF0;
						break;
					case 0x96:
						read_data = read_queue.Count() == 0 ? 0xFF : read_queue[0];
						if(read_queue.Count() != 0)
							read_queue.Remove(0, 1);
						status = 0;
						pointer = 0xF0;
						break;
					case 0x78:
					{
						EXPECT_EQ(count, 2U);
						bool has_zero = false;
						bool has_one = false;
						for(const usys_t index : search_candidates)
						{
							const bool bit = (reinterpret_cast<const byte_t*>(&roms[index])[search_bit / 8] & (1U << (search_bit % 8))) != 0;
							has_one |= bit;
							has_zero |= !bit;
						}
						const bool requested = (items[1] & 0x80) != 0;
						const bool selected = has_zero && has_one ? requested : has_one;
						status = 0;
						if(has_one && !has_zero)
							status |= 0x20;
						if(has_zero && !has_one)
							status |= 0x40;
						if(selected)
							status |= 0x80;

						TList<usys_t> remaining;
						for(const usys_t index : search_candidates)
						{
							const bool bit = (reinterpret_cast<const byte_t*>(&roms[index])[search_bit / 8] & (1U << (search_bit % 8))) != 0;
							if(bit == selected)
								remaining.Append(index);
						}
						search_candidates = remaining;
						search_bit++;
						pointer = 0xF0;
						break;
					}
					default:
						ADD_FAILURE() << "unexpected DS2482 command";
				}
				return count;
			}

			II2CBus* Bus() const final override { return nullptr; }
			u8_t Address() const final override { return 0x18; }
			ESpeedClass SpeedClass() const final override { return ESpeedClass::FULL; }
	};

	TEST(dev_i2c_ds2482, scans_single_ds18b20)
	{
		auto fake = std::unique_ptr<TFakeDS2482>(new TFakeDS2482());
		const uuid_t expected = fake->Rom().uuid;
		TDS2482Bus bus(std::move(fake));
		const TList<uuid_t> found = bus.Scan();
		ASSERT_EQ(found.Count(), 1U);
		EXPECT_EQ(found[0], expected);
	}

	TEST(dev_i2c_ds2482, scans_multiple_ds18b20)
	{
		auto fake = std::unique_ptr<TFakeDS2482>(new TFakeDS2482(2));
		const uuid_t expected_a = fake->Rom(0).uuid;
		const uuid_t expected_b = fake->Rom(1).uuid;
		TDS2482Bus bus(std::move(fake));
		const TList<uuid_t> found = bus.Scan();
		ASSERT_EQ(found.Count(), 2U);
		bool found_a = false;
		bool found_b = false;
		for(const uuid_t uuid : found)
		{
			found_a |= uuid == expected_a;
			found_b |= uuid == expected_b;
		}
		EXPECT_TRUE(found_a);
		EXPECT_TRUE(found_b);
	}

	TEST(dev_i2c_ds2482, ds18b20_parasitic_conversion_uses_strong_pullup)
	{
		auto fake = std::unique_ptr<TFakeDS2482>(new TFakeDS2482());
		TFakeDS2482* const fake_ptr = fake.get();
		const uuid_t uuid = fake->Rom().uuid;
		TDS2482Bus bus(std::move(fake));
		TDS18X20 sensor(bus.ClaimDevice(uuid), EPowerSource::AUTO_DETECT);
		sensor.TriggerConversion();
		EXPECT_TRUE(fake_ptr->conversion_used_strong_pullup);
		EXPECT_GT(bus.PausedUntil(), el1::system::time::TTime::Now(el1::system::time::EClock::MONOTONIC));
	}

	TEST(dev_i2c_ds2482, ds18b20_negative_temperature_is_signed)
	{
		auto fake = std::unique_ptr<TFakeDS2482>(new TFakeDS2482());
		const uuid_t uuid = fake->Rom().uuid;
		TDS2482Bus bus(std::move(fake));
		TDS18X20 sensor(bus.ClaimDevice(uuid), EPowerSource::DEDICATED);
		sensor.Fetch();
		EXPECT_FLOAT_EQ(sensor.Temperature(), -10.5F);
	}

	TEST(dev_i2c_ds2482, ds18s20_negative_temperature_is_signed)
	{
		auto fake = std::unique_ptr<TFakeDS2482>(new TFakeDS2482(1, EModel::DS18S20));
		const uuid_t uuid = fake->Rom().uuid;
		TDS2482Bus bus(std::move(fake));
		TDS18X20 sensor(bus.ClaimDevice(uuid), EPowerSource::DEDICATED);
		sensor.Fetch();
		EXPECT_FLOAT_EQ(sensor.Temperature(), -10.5F);
	}

	TEST(dev_i2c_ds2482, ds18b20_is_configured_for_12_bit_resolution)
	{
		auto fake = std::unique_ptr<TFakeDS2482>(new TFakeDS2482());
		TFakeDS2482* const fake_ptr = fake.get();
		const uuid_t uuid = fake->Rom().uuid;
		TDS2482Bus bus(std::move(fake));
		TDS18X20 sensor(bus.ClaimDevice(uuid), EPowerSource::DEDICATED);
		ASSERT_EQ(fake_ptr->ScratchpadWrites().Count(), 3U);
		EXPECT_EQ(fake_ptr->ScratchpadWrites()[0], 0xFF);
		EXPECT_EQ(fake_ptr->ScratchpadWrites()[1], 0xFF);
		EXPECT_EQ(fake_ptr->ScratchpadWrites()[2], 0x7F);
	}
	TEST(dev_w1, uuid_string_roundtrip)
	{
		const uuid_t uuid = uuid_t::FromString("28|01:02:03:04:05:06|9e");
		EXPECT_EQ(uuid.ToString(), "28|01:02:03:04:05:06|9e");
		EXPECT_THROW(uuid_t::FromString("28|01:02:03:04:05:06|9e trailing"), el1::error::TInvalidArgumentException);
		EXPECT_THROW(uuid_t::FromString("28|01:02:03:04:05:0g|9e"), el1::error::TInvalidArgumentException);
	}

}
