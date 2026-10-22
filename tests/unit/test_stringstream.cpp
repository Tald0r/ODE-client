//----------------------------------------------------------------------
// test_stringstream.cpp
//----------------------------------------------------------------------
//
// StringStream formats packet fields for toString() logging through a
// family of sprintf-into-stack-buffer operators. Task 1.3 of
// docs/reviews/RESTRUCTURING.md: the float overload printed "%f" into
// char[12], which any value >= 10,000 overruns ("10000.000000" is 12
// characters plus the terminator), and the double overload printed
// into char[22], overrun from about 1e15. The server repository found
// and fixed the identical family on its side (its Exchange reconcile
// notes); these tests pin the fixed contract on the client.
//
// Per the house rule for memory-safety tests, the assertions state the
// observable contract - the exact formatted text, or its exact length
// where the digits depend on binary-to-decimal conversion. Before the
// fix, the boundary cases here smash the stack instead: /RTC1 or ASan
// aborts the run, which is the defect proving itself.
//
//----------------------------------------------------------------------

#include "test_framework.h"

#include "StringStream.h"

#include <string>

TEST(StringStream, FloatAtTenThousandFormatsExactly)
{
	// 12 characters - one more than the old char[12] could terminate.
	StringStream ss;
	ss << 10000.0f;
	CHECK(ss.toString() == std::string("10000.000000"));
}

TEST(StringStream, LargeFloatFormatsInFull)
{
	// float(1e30) has a 31-digit integer part under %f, plus ".000000":
	// 38 characters. Well past the old buffer; must come through whole.
	StringStream ss;
	ss << 1.0e30f;
	CHECK_EQ(38, ss.toString().size());
	CHECK_EQ('1', ss.toString()[0]);
}

TEST(StringStream, DoubleAtOneQuadrillionFormatsExactly)
{
	// 1e15 is exactly representable: 23 characters, one more than the
	// old char[22] could terminate.
	StringStream ss;
	ss << 1.0e15;
	CHECK(ss.toString() == std::string("1000000000000000.000000"));
}

TEST(StringStream, HugeDoubleFormatsInFull)
{
	// %f of 1e300: 301 integer digits plus ".000000" - 308 characters.
	// The fixed buffer must hold the worst representable double.
	StringStream ss;
	ss << 1.0e300;
	CHECK_EQ(308, ss.toString().size());
	CHECK_EQ('1', ss.toString()[0]);
}

TEST(StringStream, IntegerBoundariesFormatExactly)
{
	{
		StringStream ss;
		ss << (int)-2147483647 - 1;
		CHECK(ss.toString() == std::string("-2147483648"));
	}
	{
		StringStream ss;
		ss << (uint)4294967295u;
		CHECK(ss.toString() == std::string("4294967295"));
	}
	{
		StringStream ss;
		ss << (ulonglong)18446744073709551615ULL;
		CHECK(ss.toString() == std::string("18446744073709551615"));
	}
}

TEST(StringStream, ShortBoundariesFormatExactly)
{
	{
		StringStream ss;
		ss << (short)-32768;
		CHECK(ss.toString() == std::string("-32768"));
	}
	{
		StringStream ss;
		ss << (ushort)65535;
		CHECK(ss.toString() == std::string("65535"));
	}
}

TEST(StringStream, MixedInsertionsConcatenate)
{
	StringStream ss;
	ss << "hp:" << (int)42 << "," << 10000.0f;
	CHECK(ss.toString() == std::string("hp:42,10000.000000"));
}

TEST(StringStream, SizeAccumulatorSurvivesSixtyFourKiB)
{
	// m_Size was ushort: 64 insertions of 1024 characters total exactly
	// 65,536, wrapping the accumulator to 0 - isEmpty() then reported an
	// empty stream over real content and toString()'s reserve()
	// under-reserved. Now size_t.
	std::string chunk(1024, 'x');

	StringStream ss;
	for (int i = 0; i < 64; i++)
		ss << chunk.c_str();

	CHECK(!ss.isEmpty());
	CHECK_EQ(65536, ss.toString().size());
}
