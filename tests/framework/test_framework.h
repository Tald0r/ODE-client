//----------------------------------------------------------------------
// test_framework.h
//----------------------------------------------------------------------
//
// Minimal self-registering unit test framework.
//
// The project targets C++20 and has no test dependency available through
// vcpkg, so this replaces GoogleTest with the smallest thing that supports
// the workflow we need: declare a test, assert inside it, get a non-zero
// exit code when something fails so CTest reports it.
//
// Usage:
//
//      TEST(ColorDraw, PreservesBlue)
//      {
//          CHECK_EQ(0x001F, ColorDraw::Convert565to555(0x001F) & 0x001F);
//      }
//
// Tests register themselves during static initialisation, so adding a test
// never requires editing a runner.
//
// Note on memory-safety tests: an out-of-bounds read in C++ usually returns
// garbage rather than failing an assertion, so tests that cover buffer
// handling are written to assert the observable contract (a rejected input,
// a preserved value) and are additionally intended to be run under a
// sanitizer build, where the invalid access itself aborts the process.
//
//----------------------------------------------------------------------

#ifndef __TEST_FRAMEWORK_H__
#define __TEST_FRAMEWORK_H__

namespace testfw {

typedef void (*TestFn)();

//----------------------------------------------------------------------
// Registers one test case during static initialisation.
//----------------------------------------------------------------------
struct Registrar
{
	Registrar(const char* suite, const char* name, TestFn fn);
};

//----------------------------------------------------------------------
// Check accounting, called by the CHECK macros below.
//----------------------------------------------------------------------
void	RecordCheck();
void	RecordFailure(const char* file, int line, const char* expr);
void	RecordFailureInt(const char* file, int line, const char* expr,
			 long long expected, long long actual);

//----------------------------------------------------------------------
// Runs every registered test. Returns 0 when the whole suite passes and 1
// when a test failed, the process's exit code. With TEST_FILTER set, only
// the tests whose "Suite.Name" starts with it run; a filter that names
// none returns 2.
//----------------------------------------------------------------------
int	RunAll();

} // namespace testfw

//----------------------------------------------------------------------
// Test declaration
//----------------------------------------------------------------------
#define TEST(suite, name)						\
	static void suite##_##name##_Body();				\
	static ::testfw::Registrar suite##_##name##_Registrar(		\
		#suite, #name, &suite##_##name##_Body);			\
	static void suite##_##name##_Body()

//----------------------------------------------------------------------
// Assertions
//----------------------------------------------------------------------
#define CHECK(expr)							\
	do {								\
		::testfw::RecordCheck();				\
		if (!(expr))						\
			::testfw::RecordFailure(__FILE__, __LINE__, #expr); \
	} while (0)

#define CHECK_EQ(expected, actual)					\
	do {								\
		::testfw::RecordCheck();				\
		const long long __e = (long long)(expected);		\
		const long long __a = (long long)(actual);		\
		if (__e != __a)						\
			::testfw::RecordFailureInt(__FILE__, __LINE__,	\
				#expected " == " #actual, __e, __a);	\
	} while (0)

#endif // __TEST_FRAMEWORK_H__
