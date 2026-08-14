#include <gtest/gtest.h>
#include <el1/dev_modbus.hpp>

using namespace ::testing;

namespace
{
	using namespace el1::dev::modbus;
	using namespace el1::io::types;

	TEST(TModBusFrameBuffer, ComputesKnownCRC16)
	{
		const byte_t data[] = {'1', '2', '3', '4', '5', '6', '7', '8', '9'};
		EXPECT_EQ(TFrameBuffer::ComputeCRC(sizeof(data), data), 0x4B37);
	}

	TEST(TModBusFrameBuffer, WritesCRCLowByteFirst)
	{
		TFrameBuffer frame;
		const byte_t data[] = {'1', '2', '3', '4', '5', '6', '7', '8', '9'};
		for(const byte_t value : data)
			frame.WriteByte(value);

		frame.WriteCRC();

		ASSERT_EQ(frame.pos_write, sizeof(data) + 2);
		EXPECT_EQ(frame.buffer[sizeof(data) + 0], 0x37);
		EXPECT_EQ(frame.buffer[sizeof(data) + 1], 0x4B);
		EXPECT_NO_THROW(frame.VerifyCRC());
	}

	TEST(TModBusFrameBuffer, ClearsUnusedCoilBits)
	{
		TFrameBuffer frame;
		const bool values[] = {true, false, true};
		frame.WriteArray(3, values);

		ASSERT_EQ(frame.pos_write, 2);
		EXPECT_EQ(frame.buffer[0], 1);
		EXPECT_EQ(frame.buffer[1], 0x05);
	}

	TEST(TModBusFrameBuffer, WritesWordsBigEndian)
	{
		TFrameBuffer frame;
		frame.WriteWord(0x1234);

		ASSERT_EQ(frame.pos_write, 2);
		EXPECT_EQ(frame.buffer[0], 0x12);
		EXPECT_EQ(frame.buffer[1], 0x34);
	}
}
