//----------------------------------------------------------------------
// test_request_file_manager.cpp
//----------------------------------------------------------------------
// Files another player asked this client for (a profile picture and its
// index, as CRRequestHandler queues them) go out over that player's peer connection, one at a time: an
// RCRequestedFile announcing the file and its size, the file's bytes in
// 4K chunks, and, after the last file, RCRequestVerify(PROFILE_DONE).
// The peer is real and connected over loopback; what its far end
// receives is decoded here.
//----------------------------------------------------------------------
#include "test_framework.h"
#include "packet_stream_access.h"
#include "RequestFileManager.h"
#include "RequestServerPlayer.h"
#include "ServerSocket.h"
#include "Socket.h"
#include "SocketAPI.h"
#include "SocketImpl.h"
#include "SocketInputStream.h"
#include "Rpackets/RCRequestedFile.h"
#include "Rpackets/RCRequestVerify.h"

#ifdef _WIN32
#include <winsock.h>
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#endif

#include <string.h>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <memory>
#include <string>
#include <thread>
#include <vector>

namespace {

uint BoundPort(SOCKET s)
{
	struct sockaddr_in addr;
	memset(&addr, 0, sizeof(addr));
#ifdef _WIN32
	int len = (int)sizeof(addr);
#else
	socklen_t len = (socklen_t)sizeof(addr);
#endif
	if (getsockname(s, (struct sockaddr*)&addr, &len) != 0)
		return 0;
	return ntohs(addr.sin_port);
}

std::vector<char> Bytes(size_t size, int seed)
{
	std::vector<char> bytes(size);
	for (size_t i = 0; i < size; i++)
		bytes[i] = static_cast<char>((i * 7 + seed) & 0xff);
	return bytes;
}

struct World
{
	bool sockets = (EnsureSocketsInitialised(), true);
	// A directory of this run's own, so runs sharing a temp directory
	// cannot remove each other's files.
	std::filesystem::path dir = std::filesystem::temp_directory_path() / ("de_request_file_manager_"
		+ std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
	ServerSocket listener{0};
	std::unique_ptr<Socket> client;
	RequestServerPlayer* peer = nullptr;
	RequestFileManager manager;

	World()
	{
		std::filesystem::create_directories(dir);
		listener.setNonBlocking(true);
		client.reset(new Socket("127.0.0.1", BoundPort(listener.getSOCKET())));
		client->connect();
		Socket* accepted = NULL;
		for (int i = 0; i < 200 && accepted == NULL; i++)
		{
			accepted = listener.accept();
			if (accepted == NULL)
				std::this_thread::sleep_for(std::chrono::milliseconds(10));
		}
		CHECK(accepted != NULL);
		if (accepted == NULL) return;
		accepted->setNonBlocking(true);
		peer = new RequestServerPlayer(accepted);
		peer->setName("Nyx");
		peer->setPlayerStatus(CPS_REQUEST_SERVER_NORMAL);
	}
	~World()
	{
		manager.Release();
		if (peer != nullptr)
		{
			peer->setPlayerStatus(CPS_END_SESSION);
			delete peer;
		}
		std::error_code ignored;
		std::filesystem::remove_all(dir, ignored);
	}

	std::string File(const char* name, const std::vector<char>& bytes)
	{
		const std::string path = (dir / name).string();
		std::ofstream(path, std::ios::binary).write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
		return path;
	}

	// Calls SendOtherRequest until the request is gone, flushing after each
	// call; returns what each call answered.
	std::vector<bool> SendAll()
	{
		std::vector<bool> answers;
		for (int i = 0; i < 100 && manager.HasOtherRequest("Nyx"); i++)
		{
			answers.push_back(manager.SendOtherRequest("Nyx", peer));
			peer->processOutput();
		}
		return answers;
	}

	// Everything the far end has received, once the stream goes quiet.
	std::vector<unsigned char> Received()
	{
		std::vector<unsigned char> bytes;
		for (int quiet = 0; quiet < 20; )
		{
			peer->processOutput();
			const uint n = client->available();
			if (n == 0)
			{
				quiet++;
				std::this_thread::sleep_for(std::chrono::milliseconds(5));
				continue;
			}
			quiet = 0;
			std::vector<unsigned char> chunk(n);
			const uint got = client->receive(chunk.data(), n);
			bytes.insert(bytes.end(), chunk.begin(), chunk.begin() + got);
		}
		return bytes;
	}
};

// A packet frame (id, size, sequence) read off the stream into a packet.
template<class PacketType>
void ReadFrame(SocketInputStream& stream, PacketType& packet)
{
	PacketID_t id = 0;
	PacketSize_t size = 0;
	BYTE sequence = 0;
	stream.read(id);
	stream.read(size);
	stream.read(sequence);
	CHECK_EQ(static_cast<long long>(packet.getPacketID()), static_cast<long long>(id));
	packet.read(stream);
	CHECK_EQ(static_cast<long long>(size), static_cast<long long>(packet.getPacketSize()));
}

void ExpectAnnounced(SocketInputStream& stream, REQUEST_FILE_TYPE type, const std::string& path, DWORD size)
{
	RCRequestedFile announced;
	ReadFrame(stream, announced);
	CHECK_EQ(1, static_cast<long long>(announced.getListNum()));
	std::unique_ptr<RCRequestedFileInfo> info(announced.popInfo());
	CHECK(info != nullptr);
	if (info == nullptr) return;
	CHECK_EQ(static_cast<long long>(type), static_cast<long long>(info->getRequestFileType()));
	CHECK_EQ(0, static_cast<long long>(info->getVersion()));
	CHECK(info->getFilename() == path);
	CHECK_EQ(static_cast<long long>(size), static_cast<long long>(info->getFileSize()));
}

void ExpectBytes(SocketInputStream& stream, const std::vector<char>& expected)
{
	std::vector<char> got(expected.size());
	if (!expected.empty())
		stream.read(got.data(), static_cast<uint>(got.size()));
	CHECK(got == expected);
}

void ExpectDone(SocketInputStream& stream)
{
	RCRequestVerify done;
	ReadFrame(stream, done);
	CHECK_EQ(static_cast<long long>(REQUEST_VERIFY_PROFILE_DONE), static_cast<long long>(done.getCode()));
}

struct Decoder
{
	Socket socket{(EnsureSocketsInitialised(), new SocketImpl())};
	SocketInputStream stream{&socket, 65536};
	explicit Decoder(const std::vector<unsigned char>& bytes)
	{
		SocketInputStreamTestAccess::Preload(stream, bytes.data(), static_cast<unsigned int>(bytes.size()));
	}
};

} // namespace

TEST(RequestFileManager, RequestedFilesGoOutAnnouncedInChunksThenDone)
{
	World world;
	// A chunk and a byte short of another, then exactly one chunk: a read
	// of a whole chunk is not yet the end, the empty read after it is.
	const std::vector<char> index = Bytes(8191, 1);
	const std::vector<char> picture = Bytes(4096, 2);
	const std::string indexPath = world.File("index.bin", index);
	const std::string picturePath = world.File("picture.bin", picture);
	RequestSendInfo* request = new RequestSendInfo("Nyx");
	// The index, then the picture, as CRRequestHandler queues them.
	request->AddSendFileInfo(new SendFileInfo(indexPath.c_str(), REQUEST_FILE_PROFILE_INDEX));
	request->AddSendFileInfo(new SendFileInfo(picturePath.c_str(), REQUEST_FILE_PROFILE));
	CHECK(world.manager.AddOtherRequest(request));
	CHECK(world.manager.HasOtherRequest("Nyx"));

	// Each file is announced by a call that answers false, then sent by
	// calls that answer true; the last one forgets the request.
	const std::vector<bool> answers = world.SendAll();
	CHECK(answers == std::vector<bool>({false, true, true, false, true, true}));
	CHECK(!world.manager.HasOtherRequest("Nyx"));

	Decoder decoder(world.Received());
	ExpectAnnounced(decoder.stream, REQUEST_FILE_PROFILE_INDEX, indexPath, 8191);
	ExpectBytes(decoder.stream, index);
	ExpectAnnounced(decoder.stream, REQUEST_FILE_PROFILE, picturePath, 4096);
	ExpectBytes(decoder.stream, picture);
	ExpectDone(decoder.stream);
	CHECK(decoder.stream.isEmpty());
}

TEST(RequestFileManager, AFileThatCannotBeOpenedIsAnnouncedEmptyAndTheRequestStillEnds)
{
	World world;
	const std::string missing = (world.dir / "missing.bin").string();
	RequestSendInfo* request = new RequestSendInfo("Nyx");
	request->AddSendFileInfo(new SendFileInfo(missing.c_str(), REQUEST_FILE_PROFILE));
	CHECK(world.manager.AddOtherRequest(request));
	CHECK(world.SendAll() == std::vector<bool>({false, true}));
	CHECK(!world.manager.HasOtherRequest("Nyx"));

	Decoder decoder(world.Received());
	ExpectAnnounced(decoder.stream, REQUEST_FILE_PROFILE, missing, 0);
	ExpectDone(decoder.stream);
	CHECK(decoder.stream.isEmpty());
}

TEST(RequestFileManager, OneRequestPerPlayerAndARemovedOrUnknownOneSendsNothing)
{
	World world;
	const std::string picturePath = world.File("picture.bin", Bytes(100, 3));
	const std::string otherPath = world.File("other.bin", Bytes(50, 4));
	RequestSendInfo* first = new RequestSendInfo("Nyx");
	first->AddSendFileInfo(new SendFileInfo(picturePath.c_str(), REQUEST_FILE_PROFILE));
	CHECK(world.manager.AddOtherRequest(first));
	// A second request from the same player is refused (and freed); the
	// first is the one sent.
	RequestSendInfo* second = new RequestSendInfo("Nyx");
	second->AddSendFileInfo(new SendFileInfo(otherPath.c_str(), REQUEST_FILE_PROFILE));
	CHECK(!world.manager.AddOtherRequest(second));
	world.SendAll();
	Decoder sent(world.Received());
	ExpectAnnounced(sent.stream, REQUEST_FILE_PROFILE, picturePath, 100);

	// A removed request, or a player who asked for nothing, sends nothing.
	RequestSendInfo* again = new RequestSendInfo("Nyx");
	again->AddSendFileInfo(new SendFileInfo(picturePath.c_str(), REQUEST_FILE_PROFILE));
	CHECK(world.manager.AddOtherRequest(again));
	CHECK(world.manager.RemoveOtherRequest("Nyx"));
	CHECK(!world.manager.RemoveOtherRequest("Nyx"));
	CHECK(!world.manager.HasOtherRequest("Nyx"));
	CHECK(!world.manager.SendOtherRequest("Nyx", world.peer));
	CHECK(!world.manager.SendOtherRequest("Vanda", world.peer));
	world.peer->processOutput();
	CHECK(world.Received().empty());
}
