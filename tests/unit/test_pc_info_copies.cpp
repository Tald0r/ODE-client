#include "test_framework.h"
#include "PCOustersInfo3.h"
#include "PCSlayerInfo3.h"
#include "PCVampireInfo3.h"
#include <cstring>
#include <new>

// A zone view's slayer, vampire or ouster details copy whole: the packets
// that carry them take them by value. Each copy is built over bytes that
// are not zero, so a member the copy leaves out reads them back. The
// enum-typed details are set first: copying one unset loads an invalid
// value, which Clang's UBSan stops at.
namespace {

template<class Info>
unsigned UnionIDOfACopy(const Info& original)
{
	alignas(Info) unsigned char bytes[sizeof(Info)];
	std::memset(bytes, 0xAA, sizeof(bytes));
	Info* copy = new (bytes) Info(original);
	const unsigned unionID = copy->getUnionID();
	copy->~Info();
	return unionID;
}

} // namespace

TEST(PCInfoCopies, ACopyKeepsTheUnion)
{
	PCSlayerInfo3 slayer;
	slayer.setSex(FEMALE);
	slayer.setUnionID(7);
	CHECK_EQ(7u, UnionIDOfACopy(slayer));
	PCVampireInfo3 vampire;
	vampire.setSex(FEMALE);
	vampire.setUnionID(8);
	CHECK_EQ(8u, UnionIDOfACopy(vampire));
	PCOustersInfo3 ousters;
	ousters.setSex(FEMALE);
	ousters.setCoatType(OUSTERS_COAT_BASIC);
	ousters.setArmType(OUSTERS_ARM_CHAKRAM);
	ousters.setSylphType(OUSTERS_SYLPH_OFF);
	ousters.setUnionID(9);
	CHECK_EQ(9u, UnionIDOfACopy(ousters));
}
