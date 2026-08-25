#include "../dev_usb_cleware_ampel.hpp"
#include <gtest/gtest.h>

namespace
{
	using namespace el1::dev::usb::cleware;

	TEST(dev_usb_cleware_ampel, BuildReport)
	{
		const report_t red_on = TAmpel::BuildReport(EColor::RED, ELightState::ON);
		EXPECT_EQ(red_on.report_id, 0x00);
		EXPECT_EQ(red_on.payload_prefix, 0x00);
		EXPECT_EQ(red_on.color, 0x10);
		EXPECT_EQ(red_on.state, 0x01);

		const report_t yellow_off = TAmpel::BuildReport(EColor::YELLOW, ELightState::OFF);
		EXPECT_EQ(yellow_off.report_id, 0x00);
		EXPECT_EQ(yellow_off.payload_prefix, 0x00);
		EXPECT_EQ(yellow_off.color, 0x11);
		EXPECT_EQ(yellow_off.state, 0x00);

		const report_t green_on = TAmpel::BuildReport(EColor::GREEN, ELightState::ON);
		EXPECT_EQ(green_on.report_id, 0x00);
		EXPECT_EQ(green_on.payload_prefix, 0x00);
		EXPECT_EQ(green_on.color, 0x12);
		EXPECT_EQ(green_on.state, 0x01);

		const report_t red_blink = TAmpel::BuildReport(EColor::RED, ELightState::BLINK_500_MS);
		EXPECT_EQ(red_blink.state, 0x10);

		const report_t yellow_blink = TAmpel::BuildReport(EColor::YELLOW, ELightState::BLINK_1_S);
		EXPECT_EQ(yellow_blink.state, 0x11);

		const report_t green_blink = TAmpel::BuildReport(EColor::GREEN, ELightState::BLINK_2_S);
		EXPECT_EQ(green_blink.state, 0x12);
	}

	TEST(dev_usb_cleware_ampel, ReportLayout)
	{
		EXPECT_EQ(sizeof(report_t), 4U);
	}
}
