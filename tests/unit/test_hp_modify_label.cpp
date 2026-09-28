#include "test_framework.h"
#include "Client/HPModifyLabel.h"

#include <string>

TEST(HPModifyLabel, DamageIsSignedAndLightRed)
{
	char str[128] = "";
	COLORREF color = 0;
	FormatHPModifyLabel(-7, str, color);
	CHECK(std::string(str) == "-7");
	CHECK_EQ(RGB(255, 150, 150), color);
}

TEST(HPModifyLabel, HealIsSignedAndLightGreen)
{
	char str[128] = "";
	COLORREF color = 0;
	FormatHPModifyLabel(12, str, color);
	CHECK(std::string(str) == "+12");
	CHECK_EQ(RGB(150, 255, 150), color);
}

TEST(HPModifyLabel, ZeroIsDrawnLikeAHeal)
{
	char str[128] = "";
	COLORREF color = 0;
	FormatHPModifyLabel(0, str, color);
	CHECK(std::string(str) == "+0");
	CHECK_EQ(RGB(150, 255, 150), color);
}
