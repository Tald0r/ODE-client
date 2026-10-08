#include "test_framework.h"

#include "DebugLog.h"
#include "CrtCompat.h"
#include "Cpackets/CLLogin.h"
#include "Player.h"
#include "Socket.h"
#include "SocketImpl.h"
#include "packet_stream_access.h"

#include <atomic>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <initializer_list>
#include <optional>
#include <string>
#include <sstream>
#include <thread>
#include <unordered_set>

namespace {

class TraceEnvironment
{
public:
	explicit TraceEnvironment(const char* value)
	{
		m_Previous = Basic::GetEnvironment("DARKEDEN_TRACE");
		Set(value);
	}
	~TraceEnvironment() { Set(m_Previous ? m_Previous->c_str() : nullptr); }
private:
	static void Set(const char* value)
	{
#ifdef _WIN32
		_putenv_s("DARKEDEN_TRACE", value ? value : "");
#else
		if (value) setenv("DARKEDEN_TRACE", value, 1);
		else unsetenv("DARKEDEN_TRACE");
#endif
	}
	std::optional<std::string> m_Previous;
};

class LogFile
{
public:
	LogFile() : m_Path(std::filesystem::temp_directory_path() / "darkeden-diagnostic-policy.log")
	{
		log_init();
		log_set_console_output(false);
		log_set_file_output(m_Path.string().c_str());
	}
	~LogFile()
	{
		log_set_file_output(nullptr);
		log_set_console_output(true);
		log_cleanup();
		std::error_code error;
		std::filesystem::remove(m_Path, error);
	}
	std::string Text() const
	{
		std::ifstream input(m_Path);
		return std::string(std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>());
	}
private:
	std::filesystem::path m_Path;
};

class LogPlayer : public Player
{
public:
	LogPlayer() : Player(new Socket((EnsureSocketsInitialised(), new SocketImpl()))) {}
	std::vector<unsigned char> Bytes() const { return SocketOutputStreamTestAccess::Bytes(*m_pOutputStream); }
};

class SecretLogin : public CLLogin
{
public:
#ifdef __DEBUG_OUTPUT__
	std::string toString() const override
	{
		++descriptions;
		return CLLogin::toString();
	}
#endif
	mutable int descriptions = 0;
};

} // namespace

TEST(DebugLogPolicy, DefaultKeepsInformationWarningsAndErrorsWithoutRoutineDiagnostics)
{
	TraceEnvironment environment(nullptr);
	LogFile log;
	LOG_INFO("important-information");
	LOG_WARN("important-warning");
	LOG_ERROR("important-error");
	DEBUG_ADD("routine-message");
	DEBUG_ADD_FORMAT("routine-formatted %d", 7);
	LOG_DEBUG("routine-debug");
	const auto text = log.Text();
	CHECK(text.find("important-information") != std::string::npos);
	CHECK(text.find("important-warning") != std::string::npos);
	CHECK(text.find("important-error") != std::string::npos);
	CHECK(text.find("routine-") == std::string::npos);
}

TEST(DebugLogPolicy, FilteredDiagnosticsDoNotEvaluateArguments)
{
	TraceEnvironment environment(nullptr);
	LogFile log;
	log_set_level(LOG_LEVEL_INFO);
	int evaluated = 0;
	LOG_DEBUG("debug %d", ++evaluated);
	DEBUG_ADD_FORMAT("legacy %d", ++evaluated);
	CHECK_EQ(0, evaluated);
	CHECK(log.Text().empty());
	log_set_level(LOG_LEVEL_DEBUG);
	LOG_DEBUG("debug %d", ++evaluated);
	DEBUG_ADD_FORMAT("legacy %d", ++evaluated);
	CHECK_EQ(2, evaluated);
	CHECK(log.Text().find("debug 1") != std::string::npos);
	CHECK(log.Text().find("legacy 2") != std::string::npos);
}

TEST(DebugLogPolicy, TraceRequiresAnExplicitOneAndPreservesLegacySeverities)
{
	for (const char* setting : {"", "0", "true", "01", "1"})
	{
		TraceEnvironment environment(setting);
		LogFile log;
		DEBUG_ADD("routine-trace");
		DEBUG_ADD_WAR("legacy-warning");
		DEBUG_ADD_FORMAT_ERR("legacy-error %d", 3);
		const auto text = log.Text();
		CHECK((text.find("routine-trace") != std::string::npos) == (std::string(setting) == "1"));
		CHECK(text.find("[WARN ") != std::string::npos);
		CHECK(text.find("legacy-warning") != std::string::npos);
		CHECK(text.find("[ERROR]") != std::string::npos);
		CHECK(text.find("legacy-error 3") != std::string::npos);
	}
}

TEST(DebugLogPolicy, SendingLoginNeverLogsCredentialsEvenWhenTracing)
{
	TraceEnvironment environment("1");
	LogFile log;
	log_set_level(LOG_LEVEL_DEBUG);
	SecretLogin packet;
	packet.setID("private-account");
	packet.setPassword("private-password");
	LogPlayer traced;
	traced.sendPacket(&packet);
	CHECK_EQ(0, packet.descriptions);
	const auto text = log.Text();
	CHECK(text.find("private-account") == std::string::npos);
	CHECK(text.find("private-password") == std::string::npos);
#ifdef __DEBUG_OUTPUT__
	CHECK(text.find("[Send] packet id=") != std::string::npos);
#endif
	log_set_level(LOG_LEVEL_INFO);
	LogPlayer quiet;
	quiet.sendPacket(&packet);
	CHECK(traced.Bytes() == quiet.Bytes());
	CHECK_EQ(0, packet.descriptions);
}

TEST(DebugLogPolicy, ConcurrentLevelChangesPreserveCompleteWarningRecords)
{
	TraceEnvironment environment(nullptr);
	LogFile log;
	std::atomic<bool> start{false};
	const auto wait = [&] {
		while (!start.load(std::memory_order_acquire)) std::this_thread::yield();
	};
	std::thread levels([&] {
		wait();
		for (int i = 0; i < 3000; ++i)
			log_set_level(i % 2 == 0 ? LOG_LEVEL_DEBUG : LOG_LEVEL_INFO);
	});
	const auto write = [&](int worker) {
		wait();
		for (int step = 0; step < 200; ++step)
		{
			LOG_DEBUG("debug-worker=%d step=%d", worker, step);
			LOG_WARN("warning-worker=%d step=%d", worker, step);
		}
	};
	std::thread first(write, 1);
	std::thread second(write, 2);
	start.store(true, std::memory_order_release);
	first.join();
	second.join();
	levels.join();

	std::unordered_set<std::string> expected;
	for (int worker = 1; worker <= 2; ++worker)
		for (int step = 0; step < 200; ++step)
			expected.insert("warning-worker=" + std::to_string(worker) + " step=" + std::to_string(step));
	std::istringstream lines(log.Text());
	std::string line;
	while (std::getline(lines, line))
	{
		const auto warning = line.find("warning-worker=");
		if (warning != std::string::npos)
		{
			CHECK(line.find("[WARN ") != std::string::npos);
			CHECK_EQ(1, expected.erase(line.substr(warning)));
		}
		else
		{
			CHECK(line.find("[DEBUG]") != std::string::npos);
			CHECK(line.find("debug-worker=") != std::string::npos);
		}
	}
	CHECK(expected.empty());
}
