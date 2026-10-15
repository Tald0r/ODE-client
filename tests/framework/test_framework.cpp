//----------------------------------------------------------------------
// test_framework.cpp
//----------------------------------------------------------------------

#include "test_framework.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <exception>
#include <vector>

namespace testfw {

namespace {

struct TestCase
{
	const char*	suite;
	const char*	name;
	TestFn		fn;
};

//----------------------------------------------------------------------
// Held in a function-local static so registration during static
// initialisation cannot race the container's own construction.
//----------------------------------------------------------------------
std::vector<TestCase>&	Registry()
{
	static std::vector<TestCase> registry;
	return registry;
}

int	g_TotalChecks	= 0;
int	g_TestFailures	= 0;	// failed checks inside the running test

} // namespace

//----------------------------------------------------------------------
Registrar::Registrar(const char* suite, const char* name, TestFn fn)
{
	TestCase testCase;

	testCase.suite	= suite;
	testCase.name	= name;
	testCase.fn	= fn;

	Registry().push_back(testCase);
}

//----------------------------------------------------------------------
void	RecordCheck()
{
	g_TotalChecks++;
}

//----------------------------------------------------------------------
void	RecordFailure(const char* file, int line, const char* expr)
{
	g_TestFailures++;

	std::printf("      %s(%d): failed: %s\n", file, line, expr);

	// Flushed eagerly: a test covering memory handling can abort the
	// process, and buffered output would be lost with it.
	std::fflush(stdout);
}

//----------------------------------------------------------------------
void	RecordFailureInt(const char* file, int line, const char* expr,
			 long long expected, long long actual)
{
	g_TestFailures++;

	std::printf("      %s(%d): failed: %s (expected %lld, actual %lld)\n",
		    file, line, expr, expected, actual);

	std::fflush(stdout);
}

//----------------------------------------------------------------------
int	RunAll()
{
	const std::vector<TestCase>&	tests	= Registry();

	int	failedTests	= 0;

	// TEST_FILTER, when set, runs only the tests whose "Suite.Name" starts
	// with it (a mutant run's speed-up).
	const char*	filter	= std::getenv("TEST_FILTER");
	int			ran		= 0;

	std::printf("Running %d test(s)\n\n", (int)tests.size());

	for (size_t i = 0; i < tests.size(); i++)
	{
		const TestCase&	testCase = tests[i];

		if (filter != NULL)
		{
			const std::string	fullName = std::string(testCase.suite) + "." + testCase.name;
			if (fullName.compare(0, std::strlen(filter), filter) != 0)
				continue;
		}
		ran++;

		g_TestFailures = 0;

		// Announced before the test runs so a crash inside it still
		// names the test that caused it.
		std::printf("  [RUN ] %s.%s\n", testCase.suite, testCase.name);
		std::fflush(stdout);

		// A test body that throws (the packet parsers throw on bad
		// input by design) must fail as a named test, not take the
		// rest of the suite down with it.
		try {
			testCase.fn();
		} catch (const std::exception& e) {
			RecordFailure(__FILE__, __LINE__, e.what());
		} catch (...) {
			RecordFailure(__FILE__, __LINE__, "uncaught exception of unknown type");
		}

		if (g_TestFailures == 0)
		{
			std::printf("  [PASS] %s.%s\n", testCase.suite, testCase.name);
		}
		else
		{
			std::printf("  [FAIL] %s.%s (%d failed check(s))\n",
				    testCase.suite, testCase.name, g_TestFailures);

			failedTests++;
		}

		std::fflush(stdout);
	}

	std::printf("\n%d test(s), %d check(s), %d failed\n",
		    ran, g_TotalChecks, failedTests);

	return failedTests;
}

} // namespace testfw
