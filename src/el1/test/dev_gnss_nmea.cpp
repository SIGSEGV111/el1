#include <gtest/gtest.h>
#include <el1/dev_gnss_nmea.hpp>

using namespace ::testing;

namespace
{
	using namespace el1::dev::gnss::nmea;
	using namespace el1::io::types;

	TEST(dev_gnss_nmea, GgaAndRmc)
	{
		TParser parser;

		const auto gga = parser.feedSentence(U"$GPGGA,123519,4807.038,N,01131.000,E,1,08,0.9,545.4,M,46.9,M,,*47");
		ASSERT_TRUE(gga.has_value());
		EXPECT_TRUE(gga->valid);
		EXPECT_NEAR(gga->latitude_deg, 48.1173, 1e-6);
		EXPECT_NEAR(gga->longitude_deg, 11.516666667, 1e-6);
		EXPECT_EQ(gga->satellites, 8U);
		EXPECT_NEAR(gga->horizontal_dop, 0.9, 1e-9);
		EXPECT_NEAR(gga->altitude_m, 545.4, 1e-9);

		const auto rmc = parser.feedSentence(U"$GPRMC,123519,A,4807.038,N,01131.000,E,022.4,084.4,230394,003.1,W*6A");
		ASSERT_TRUE(rmc.has_value());
		EXPECT_TRUE(rmc->valid);
		EXPECT_NEAR(rmc->latitude_deg, 48.1173, 1e-6);
		EXPECT_NEAR(rmc->longitude_deg, 11.516666667, 1e-6);
		EXPECT_NEAR(rmc->speed_mps, 11.523555556, 1e-6);
		EXPECT_NEAR(rmc->heading_deg, 84.4, 1e-9);
		EXPECT_NEAR(rmc->altitude_m, 545.4, 1e-9);
	}

	TEST(dev_gnss_nmea, RejectsBadChecksum)
	{
		TParser parser;
		EXPECT_FALSE(parser.feedSentence(U"$GPGGA,123519,4807.038,N,01131.000,E,1,08,0.9,545.4,M,46.9,M,,*00").has_value());
	}

	TEST(dev_gnss_nmea, Streaming)
	{
		TParser parser;
		const char* const sentence = "$GPRMC,123519,A,4807.038,N,01131.000,E,022.4,084.4,230394,003.1,W*6A\r\n";
		std::optional<TFix> fix;
		for(const char* p = sentence; *p != '\0'; p++)
		{
			const auto result = parser.feedByte(static_cast<byte_t>(*p));
			if(result.has_value())
				fix = result;
		}
		ASSERT_TRUE(fix.has_value());
		EXPECT_TRUE(fix->valid);
		EXPECT_NEAR(fix->latitude_deg, 48.1173, 1e-6);
	}
	TEST(dev_gnss_nmea, GnTalkerFromAndroid)
	{
		TParser parser;

		const auto gga = parser.feedSentence(U"$GNGGA,065612.000,7057.9677,N,02420.5912,E,1,32,0.4,12.1,M,25.4,M,,*71");
		ASSERT_TRUE(gga.has_value());
		EXPECT_TRUE(gga->valid);
		EXPECT_NEAR(gga->latitude_deg, 70.966128333, 1e-6);
		EXPECT_NEAR(gga->longitude_deg, 24.343186667, 1e-6);
		EXPECT_NEAR(gga->horizontal_dop, 0.4, 1e-9);
		EXPECT_NEAR(gga->altitude_m, 12.1, 1e-9);
		EXPECT_EQ(gga->satellites, 32U);

		const auto rmc = parser.feedSentence(U"$GNRMC,065612.000,A,7057.9677,N,02420.5912,E,12.1,229.2,130926,,,A*41");
		ASSERT_TRUE(rmc.has_value());
		EXPECT_TRUE(rmc->valid);
		EXPECT_NEAR(rmc->speed_mps, 6.224777778, 1e-6);
		EXPECT_NEAR(rmc->heading_deg, 229.2, 1e-9);
	}

	TEST(dev_gnss_nmea, VoidFixAndGrammarValidation)
	{
		TParser parser;

		const auto void_rmc = parser.feedSentence(U"$GNRMC,065612.000,V,,,,,,,130926,,,N");
		ASSERT_TRUE(void_rmc.has_value());
		EXPECT_FALSE(void_rmc->valid);

		EXPECT_FALSE(parser.feedSentence(U"$GNGGA,065612.000,9960.0000,N,02420.5912,E,1,32,0.4,12.1,M,25.4,M,,").has_value());
		EXPECT_FALSE(parser.feedSentence(U"$GPGSV,1,1,01,01,45,180,40").has_value());
	}

}
