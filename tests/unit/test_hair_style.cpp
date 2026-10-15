#include "test_framework.h"
#include "PCSlayerInfo2.h"
#include "PCSlayerInfo3.h"

// A slayer's hair style arrives as a byte (PCSlayerInfo2::read and its
// siblings store it as is), and the client has three. A style past them is
// the wire's to send, so it must stay the value it is: reading it back
// must not be undefined behaviour (Clang's UBSan checks enum loads).
TEST(HairStyle, AStylePastTheThreeKeepsItsValue)
{
	for (int style : {3, 4, 255})
	{
		PCSlayerInfo2 info2;
		info2.setHairStyle(HairStyle(style));
		CHECK_EQ(style, int(info2.getHairStyle()));
		// PCSlayerInfo3 keeps the style in two outlook bits: what it reads
		// back is the style's low two bits.
		PCSlayerInfo3 info3;
		info3.setHairStyle(HairStyle(style));
		CHECK_EQ(style & 3, int(info3.getHairStyle()));
	}
}
