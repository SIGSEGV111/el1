#include <gtest/gtest.h>
#include <el1/dev_gnss_nmea.hpp>
#include <el1/io_collection_array.hpp>
#include <el1/io_stream.hpp>
#include <el1/io_text_encoding_utf8.hpp>

using namespace ::testing;

namespace
{
	using namespace el1::dev::gnss::nmea;
	using namespace el1::io::types;
	using namespace el1::io::text::encoding::utf8;
	using namespace el1::io::text::string;

	TEST(dev_gnss_nmea, GgaAndRmc)
	{
		TParser parser;

		const auto gga = parser.parseSentence(U"$GPGGA,123519,4807.038,N,01131.000,E,1,08,0.9,545.4,M,46.9,M,,*47");
		ASSERT_TRUE(gga.has_value());
		EXPECT_TRUE(gga->valid);
		EXPECT_NEAR(gga->latitude_deg, 48.1173, 1e-6);
		EXPECT_NEAR(gga->longitude_deg, 11.516666667, 1e-6);
		ASSERT_TRUE(gga->satellites.has_value());
		EXPECT_EQ(*gga->satellites, 8U);
		ASSERT_TRUE(gga->horizontal_dop.has_value());
		EXPECT_NEAR(*gga->horizontal_dop, 0.9, 1e-9);
		ASSERT_TRUE(gga->altitude_m.has_value());
		EXPECT_NEAR(*gga->altitude_m, 545.4, 1e-9);

		const auto rmc = parser.parseSentence(U"$GPRMC,123519,A,4807.038,N,01131.000,E,022.4,084.4,230394,003.1,W*6A");
		ASSERT_TRUE(rmc.has_value());
		EXPECT_TRUE(rmc->valid);
		EXPECT_NEAR(rmc->latitude_deg, 48.1173, 1e-6);
		EXPECT_NEAR(rmc->longitude_deg, 11.516666667, 1e-6);
		ASSERT_TRUE(rmc->speed_mps.has_value());
		EXPECT_NEAR(*rmc->speed_mps, 11.523555556, 1e-6);
		ASSERT_TRUE(rmc->heading_deg.has_value());
		EXPECT_NEAR(*rmc->heading_deg, 84.4, 1e-9);
		ASSERT_TRUE(rmc->altitude_m.has_value());
		EXPECT_NEAR(*rmc->altitude_m, 545.4, 1e-9);
	}

	TEST(dev_gnss_nmea, RejectsBadChecksum)
	{
		TParser parser;
		EXPECT_FALSE(parser.parseSentence(U"$GPGGA,123519,4807.038,N,01131.000,E,1,08,0.9,545.4,M,46.9,M,,*00").has_value());
	}

	TEST(dev_gnss_nmea, GnTalkerFromAndroid)
	{
		TParser parser;

		const auto gga = parser.parseSentence(U"$GNGGA,065612.000,7057.9677,N,02420.5912,E,1,32,0.4,12.1,M,25.4,M,,*71");
		ASSERT_TRUE(gga.has_value());
		EXPECT_TRUE(gga->valid);
		EXPECT_NEAR(gga->latitude_deg, 70.966128333, 1e-6);
		EXPECT_NEAR(gga->longitude_deg, 24.343186667, 1e-6);
		ASSERT_TRUE(gga->horizontal_dop.has_value());
		EXPECT_NEAR(*gga->horizontal_dop, 0.4, 1e-9);
		ASSERT_TRUE(gga->altitude_m.has_value());
		EXPECT_NEAR(*gga->altitude_m, 12.1, 1e-9);
		ASSERT_TRUE(gga->satellites.has_value());
		EXPECT_EQ(*gga->satellites, 32U);

		const auto rmc = parser.parseSentence(U"$GNRMC,065612.000,A,7057.9677,N,02420.5912,E,12.1,229.2,130926,,,A*41");
		ASSERT_TRUE(rmc.has_value());
		ASSERT_TRUE(rmc->speed_mps.has_value());
		EXPECT_NEAR(*rmc->speed_mps, 6.224777778, 1e-6);
		ASSERT_TRUE(rmc->heading_deg.has_value());
		EXPECT_NEAR(*rmc->heading_deg, 229.2, 1e-9);
	}

	TEST(dev_gnss_nmea, VoidFixAndGrammarValidation)
	{
		TParser parser;

		const auto void_rmc = parser.parseSentence(U"$GNRMC,065612.000,V,,,,,,,130926,,,N");
		ASSERT_TRUE(void_rmc.has_value());
		EXPECT_FALSE(void_rmc->valid);

		EXPECT_FALSE(parser.parseSentence(U"$GNGGA,065612.000,9960.0000,N,02420.5912,E,1,32,0.4,12.1,M,25.4,M,,").has_value());
		EXPECT_FALSE(parser.parseSentence(U"$GPGSV,1,1,01,01,45,180,40").has_value());
		EXPECT_FALSE(parser.parseSentence(U"garbage$GPGGA,000000,4807.038,N,01131.000,E,1,08,0.9,545.4").has_value());
	}

	TEST(dev_gnss_nmea, CoordinateValidation)
	{
		TParser parser;

		const auto edge = parser.parseSentence(U"$GPGGA,000000,9000.000,N,18000.000,E,1,01,,");
		ASSERT_TRUE(edge.has_value());
		EXPECT_DOUBLE_EQ(edge->latitude_deg, 90.0);
		EXPECT_DOUBLE_EQ(edge->longitude_deg, 180.0);

		EXPECT_FALSE(parser.parseSentence(U"$GPGGA,000000,9000.001,N,18000.000,E,1,01,,").has_value());
		EXPECT_FALSE(parser.parseSentence(U"$GPGGA,000000,9000.000,N,18000.001,E,1,01,,").has_value());
		EXPECT_FALSE(parser.parseSentence(U"$GPGGA,000000,4807.038,E,01131.000,E,1,08,0.9,545.4").has_value());
		EXPECT_FALSE(parser.parseSentence(U"$GPGGA,000000,4807.038,N,01131.000,N,1,08,0.9,545.4").has_value());
	}

	TEST(dev_gnss_nmea, RejectsMalformedFields)
	{
		TParser parser;

		EXPECT_FALSE(parser.parseSentence(U"$GPGGA,123519,4807.038,N,01131.000,E,1,4294967296,0.9,545.4").has_value());
		EXPECT_FALSE(parser.parseSentence(U"$GPRMC,123519,A,4807.038,N,01131.000,E,22.,084.4").has_value());
		EXPECT_FALSE(parser.parseSentence(U"$GPRMC,123519,A,4807.038,N,01131.000,E,22.4,084.4\n,trailing").has_value());
	}

	TEST(dev_gnss_nmea, StreamPipeline)
	{
		using namespace el1::io::collection::array;

		const char input[] =
			"partial sentence\r\n"
			"$GPGGA,123519,4807.038,N,01131.000,E,1,08,0.9,545.4,M,46.9,M,,*47\r\n"
			"$broken\r\n"
			"$GPRMC,123519,A,4807.038,N,01131.000,E,022.4,084.4,230394,003.1,W*6A\r\n";
		const auto bytes = array_t<const byte_t>::FromUnsafePointer(reinterpret_cast<const byte_t*>(input), sizeof(input) - 1);
		const auto fixes = bytes.Pipe()
			.Transform(TUTF8Decoder())
			.Transform(TLineReader(TParser::MAX_SENTENCE_LENGTH))
			.Transform(TParser())
			.Collect();

		ASSERT_EQ(fixes.Count(), 2U);
		ASSERT_TRUE(fixes[0].altitude_m.has_value());
		EXPECT_NEAR(*fixes[0].altitude_m, 545.4, 1e-9);
		EXPECT_FALSE(fixes[0].speed_mps.has_value());
		ASSERT_TRUE(fixes[1].altitude_m.has_value());
		EXPECT_NEAR(*fixes[1].altitude_m, 545.4, 1e-9);
		ASSERT_TRUE(fixes[1].speed_mps.has_value());
		EXPECT_NEAR(*fixes[1].speed_mps, 11.523555556, 1e-6);
	}
}
