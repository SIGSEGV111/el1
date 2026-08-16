#include <gtest/gtest.h>
#include <el1/dev_obd2_elm327.hpp>
#include <el1/io_collection_list.hpp>
#include <el1/io_stream.hpp>
#include <el1/system_waitable.hpp>
#include <cstring>
#include <string>
#include <utility>
#include <vector>

using namespace ::testing;

namespace
{
	using namespace el1::dev::obd2::elm327;
	using namespace el1::io::collection::list;
	using namespace el1::io::stream;
	using namespace el1::io::text::string;
	using namespace el1::io::types;
	using namespace el1::system::waitable;

	class TScriptStream final : public IBinarySource, public IBinarySink
	{
		private:
			class TInputReady final : public IWaitable
			{
				private:
					const TScriptStream* const owner;

				public:
					bool IsReady() const final override
					{
						return owner->read_offset < owner->input.size();
					}

					explicit TInputReady(const TScriptStream* const owner) : owner(owner)
					{
					}
			};

			std::vector<std::pair<std::string, std::string>> script;
			std::string input;
			std::string output;
			size_t read_offset;
			size_t script_offset;
			TInputReady input_ready;

		public:
			explicit TScriptStream(std::vector<std::pair<std::string, std::string>> script) :
				script(std::move(script)),
				read_offset(0),
				script_offset(0),
				input_ready(this)
			{
			}

			usys_t Read(byte_t* const arr_items, const usys_t n_items_max) final override
			{
				const size_t available = input.size() - read_offset;
				const size_t count = std::min<size_t>(available, n_items_max);
				if(count == 0)
					return 0;
				std::memcpy(arr_items, input.data() + read_offset, count);
				read_offset += count;
				if(read_offset == input.size())
				{
					input.clear();
					read_offset = 0;
				}
				return count;
			}

			usys_t Write(const byte_t* const arr_items, const usys_t n_items_max) final override
			{
				for(usys_t i = 0; i < n_items_max; i++)
				{
					const char character = static_cast<char>(arr_items[i]);
					if(character == '\r')
					{
						EXPECT_LT(script_offset, script.size());
						if(script_offset < script.size())
						{
							EXPECT_EQ(output, script[script_offset].first);
							input += script[script_offset].second;
							script_offset++;
						}
						output.clear();
					}
					else
					{
						output += character;
					}
				}
				return n_items_max;
			}

			const IWaitable* OnInputReady() const final override
			{
				return &input_ready;
			}

			bool Complete() const
			{
				return script_offset == script.size();
			}
	};

	TEST(dev_obd2_elm327, ParseReadDataByIdentifierResponse)
	{
		const TList<u8_t> data = TELM327::ParseReadDataByIdentifierResponse(U"0: 62 DD BC 01 91\r>", 0xDDBC);
		ASSERT_EQ(data.Count(), 2U);
		EXPECT_EQ(data[0], 0x01);
		EXPECT_EQ(data[1], 0x91);
		EXPECT_EQ(TELM327::ParseReadDataByIdentifierResponse(U"NO DATA\r>", 0xDDBC).Count(), 0U);
	}

	TEST(dev_obd2_elm327, InitializeAndReadDid)
	{
		TScriptStream stream({
			{ "ATZ", "vLinker MC+\r>" },
			{ "ATE0", "OK\r>" },
			{ "ATL0", "OK\r>" },
			{ "ATS0", "OK\r>" },
			{ "ATH0", "OK\r>" },
			{ "ATAL", "OK\r>" },
			{ "ATAT1", "OK\r>" },
			{ "ATCAF1", "OK\r>" },
			{ "ATCFC1", "OK\r>" },
			{ "ATSP6", "OK\r>" },
			{ "22DDBC", "62DDBC0191\r>" },
			{ "ATRV", "12.6V\r>" },
			{ "ATRV", "  13.78v \r>" },
		});
		TELM327 elm327(stream, stream, 1);
		elm327.Initialize(EProtocol::ISO_15765_4_CAN_11_500);
		const TList<u8_t> data = elm327.ReadDataByIdentifier(0xDDBC);
		ASSERT_EQ(data.Count(), 2U);
		EXPECT_EQ(data[0], 0x01);
		EXPECT_EQ(data[1], 0x91);
		EXPECT_DOUBLE_EQ(elm327.SupplyVoltage(), 12.6);
		EXPECT_DOUBLE_EQ(elm327.SupplyVoltage(), 13.78);
		EXPECT_TRUE(stream.Complete());
	}
	TEST(dev_obd2_elm327, WaitForBusActivity)
	{
		TScriptStream active_stream({
			{ "ATAR", "OK\r>" },
			{ "ATMA", "6078F6210AABBCC\r" },
			{ "", ">" },
		});
		TELM327 active_elm327(active_stream, active_stream, 1);
		EXPECT_TRUE(active_elm327.WaitForBusActivity(0.1));
		EXPECT_TRUE(active_stream.Complete());

		TScriptStream quiet_stream({
			{ "ATAR", "OK\r>" },
			{ "ATMA", "" },
			{ "", ">" },
		});
		TELM327 quiet_elm327(quiet_stream, quiet_stream, 1);
		EXPECT_FALSE(quiet_elm327.WaitForBusActivity(0.01));
		EXPECT_TRUE(quiet_stream.Complete());
	}

}
