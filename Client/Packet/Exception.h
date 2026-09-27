//////////////////////////////////////////////////////////////////////
// 
// Filename    : Exception.h 
// Written By  : reiot@ewestsoft.com
// Description :
// 
//////////////////////////////////////////////////////////////////////

#ifndef __EXCEPTION_H__
#define __EXCEPTION_H__

// include files
#include "Types.h"
#include "StringStream.h"

#if defined(__WINDOWS__) && __WINDOWS__
#pragma warning ( disable : 4786 )
#endif

#include <list>
#include <source_location>


//////////////////////////////////////////////////////////////////////
//
// struct DiagnosticSite
//
// Where a diagnostic was raised: a file and a line, either captured at
// the caller by the defaulted std::source_location or supplied
// explicitly.
//
//////////////////////////////////////////////////////////////////////

struct DiagnosticSite {

	const char *	file;
	int		line;
	const char *	function;	// NULL when only a file and a line were supplied

	DiagnosticSite ( const std::source_location & location = std::source_location::current() ) noexcept
		: file(location.file_name()),
		  line((int)location.line()),
		  function(location.function_name()) {}

	DiagnosticSite ( const char * site_file , int site_line ) noexcept
		: file(site_file),
		  line(site_line),
		  function(NULL) {}
};

//////////////////////////////////////////////////////////////////////
//
// class Throwable
//
// Exception 과 Error 의 베이스 클래스이다. 관련 메쏘드 및 데이타를
// 구현해놓고 있다.
//
//////////////////////////////////////////////////////////////////////

class Throwable {

public :

	// constructor
	Throwable () {}
	
	// constructor
	Throwable ( std::string message ) : m_Message(message) {}

	// destructor
	virtual ~Throwable () {}

	// return class's name
	virtual std::string getName () const { return "Throwable"; }

	// add function name to throwable object's function stack
	void addStack ( const std::string & file, const int line )
	{
		StringStream s;
		s << file << ":" << line;
		m_Stacks.push_front( s.toString());
	}

	// The same, with the location captured at the caller rather than
	// forwarded. This is what __END_CATCH calls.
	void addStack ( const DiagnosticSite & site = DiagnosticSite() )
	{
		addStack( site.file, site.line );
	}

	// return debug std::string - throwable object's function stack trace
	std::string getStackTrace () const
	{
		StringStream buf;
		int i = 1;

		for ( std::list<std::string>::const_iterator itr = m_Stacks.begin() ;
			  itr != m_Stacks.end() ;
			  itr ++ , i ++ ) {
			for ( int j = 0 ; j < i ; j ++ )
				buf << " ";
			buf << *itr << '\n' ;
		}
		
		return std::string( buf.toString() );
	}

	// get throwable object's message
	std::string getMessage () const { return m_Message; }
	
	// set throwable object's message
	void setMessage ( const std::string & message ) { m_Message = message; }
	
	// return debug string - throwable object's detailed information
	virtual std::string toString () const
	{
		StringStream buf;
		buf << getName() << " : " << m_Message << '\n'
			<< getStackTrace () ;
		
		return std::string( buf.toString() );
	}

private :
	
	// message string
	std::string m_Message;
	
	// function stack 
	std::list<std::string> m_Stacks;
};

//--------------------------------------------------------------------------------
// macro definition
//--------------------------------------------------------------------------------

//--------------------------------------------------------------------------------
//
// Throwable이 필요하기 때문에 아래에 정의했다.
// Exception/Error를 던지는 모든 메쏘드의 위/아래에 명시되어야 한다.
// __END_CATCH는 Throwable의 메소드 스택에 등록한 후 상위로 던지는
// 역할을 한다.
//
//--------------------------------------------------------------------------------

#ifdef NDEBUG
	#define __BEGIN_TRY ((void)0);
	#define __END_CATCH ((void)0);
#else
	#define __BEGIN_TRY \
				try {
	#define __END_CATCH \
				} catch ( Throwable & t ) { \
					t.addStack(); \
					throw; \
				}
#endif

//--------------------------------------------------------------------------------
//
// critical section
//
//--------------------------------------------------------------------------------
#define __ENTER_CRITICAL_SECTION(mutex) \
			mutex.lock(); \
			try {

#define __LEAVE_CRITICAL_SECTION(mutex) \
			} catch ( Throwable & t ) { \
				mutex.unlock(); \
				throw; \
			} \
			mutex.unlock();

//--------------------------------------------------------------------------------
//
// cout debugging
//
//--------------------------------------------------------------------------------
#if defined(NDEBUG) || defined(__WIN32__)
	#define __BEGIN_DEBUG ((void)0);
	#define __END_DEBUG ((void)0);
#elif defined(PLATFORM_POSIX) || defined(__WIN_CONSOLE__)
	#define __BEGIN_DEBUG \
				try {
	#define __END_DEBUG  \
				} catch ( Throwable & t ) { \
					std::cout << t.toString() << std::endl; \
					throw; \
				} catch ( std::exception & e ) { \
					std::cout << e.what() << std::endl; \
					throw; \
				}
#elif defined(__MFC__)
	#define __BEGIN_DEBUG \
				try {
	#define __END_DEBUG \
				} catch ( Throwable & t ) { \
					AfxMessageBox(t.toString()); \
					throw; \
				}
#endif


//////////////////////////////////////////////////////////////////////
//
// Exception
//
//////////////////////////////////////////////////////////////////////
class Exception : public Throwable {
public :
	Exception () : Throwable() {}
	Exception ( std::string msg ) : Throwable(msg) {}
	std::string getName () const { return "Exception"; }
};

	//////////////////////////////////////////////////////////////////////
	//
	// I/O Exception
	//
	// 파일, 소켓, IPC 입출력시 발생할 수 있는 예외
	//
	//////////////////////////////////////////////////////////////////////
	// 파일, 소켓, IPC 입출력시 발생할 수 있는 예외
	class IOException : public Exception {
	public :
		IOException () : Exception () {}
		IOException ( std::string msg ) : Exception (msg) {}
		std::string getName () const { return "IOException"; }
	};

		//////////////////////////////////////////////////////////////////////
		//
		// Non Blocking I/O Exception
		//
		// I/O 시 nonblocking 이 발생할 경우
		//
		//////////////////////////////////////////////////////////////////////
		class NonBlockingIOException : public IOException {
		public :
			NonBlockingIOException () : IOException () {}
			NonBlockingIOException ( std::string msg ) : IOException (msg) {}
			std::string getName () const { return "NonBlockingIOException"; }
		};
	
		//////////////////////////////////////////////////////////////////////
		//
		// Interrupted I/O Exception
		//
		// I/O 시 인터럽트가 걸린 경우
		//
		//////////////////////////////////////////////////////////////////////
		class InterruptedIOException : public IOException {
		public :
			InterruptedIOException () : IOException () {}
			InterruptedIOException ( std::string msg ) : IOException (msg) {}
			std::string getName () const { return "InterruptedIOException"; }
		};
	
		//////////////////////////////////////////////////////////////////////
		//
		// EOF Exception
		//
		// I/O 시 EOF 를 만난 경우
		//
		//////////////////////////////////////////////////////////////////////
		class EOFException : public IOException {
		public :
			EOFException () : IOException () {}
			EOFException ( std::string msg ) : IOException (msg) {}
			std::string getName () const { return "EOFException"; }
		};
	
		//////////////////////////////////////////////////////////////////////
		//
		// File Not Opened Exception 
		//
		//////////////////////////////////////////////////////////////////////
		class FileNotOpenedException : public IOException {
		public :
			FileNotOpenedException () : IOException() {}
			FileNotOpenedException ( std::string msg ) : IOException(msg) {}
			std::string getName () const { return "FileNotOpenedException"; }
		};

		//////////////////////////////////////////////////////////////////////
		//
		// File Already Exist Exception
		//
		//////////////////////////////////////////////////////////////////////
		class FileAlreadyExistException : public IOException {
		public :
			FileAlreadyExistException () : IOException() {}
			FileAlreadyExistException ( std::string msg ) : IOException(msg) {}
			std::string getName () const { return "FileAlreadyExistException"; }
		};

		//////////////////////////////////////////////////////////////////////
		//
		// File Not Exist Exception
		//
		//////////////////////////////////////////////////////////////////////
		class FileNotExistException : public IOException {
		public :
			FileNotExistException () : IOException() {}
			FileNotExistException ( std::string msg ) : IOException(msg) {}
			std::string getName () const { return "FileNotExistException"; }
		};

		//////////////////////////////////////////////////////////////////////
		//
		// Time out Exception
		//
		// 지정 시간이 지났을 경우
		//
		//////////////////////////////////////////////////////////////////////
		class TimeoutException : public IOException {
		public :
			TimeoutException () : IOException () {}
			TimeoutException ( std::string msg ) : IOException (msg) {}
			std::string getName () const { return "TimeoutException"; }
		};

		//////////////////////////////////////////////////////////////////////
		//
		// Socket Exception
		//
		// 특히 소켓에서 발생하는 예외들
		//
		//////////////////////////////////////////////////////////////////////
		class SocketException : public IOException {
		public :
			SocketException () : IOException () {}
			SocketException ( std::string msg ) : IOException (msg) {}
			std::string getName () const { return "SocketException"; }
		};
	
			//////////////////////////////////////////////////////////////////////
			//
			// Bind Exception
			//
			// bind()시 발생하는 예외
			//
			//////////////////////////////////////////////////////////////////////
			class BindException : public SocketException {
			public :
				BindException () : SocketException () {}
				BindException ( std::string msg ) : SocketException (msg) {}
				std::string getName () const { return "BindException"; }
			};
	
			//////////////////////////////////////////////////////////////////////
			//
			// Connect Exception
			//
			// 소켓 연결이 끊길 경우 ( 가장 많이 발생한다고 보면 된다. )
			//
			//////////////////////////////////////////////////////////////////////
			class ConnectException : public SocketException {
			public :
				ConnectException () : SocketException () {}
				ConnectException ( std::string msg ) : SocketException (msg) {}
				std::string getName () const { return "ConnectException"; }
			};
			
		//////////////////////////////////////////////////////////////////////
		//
		// Protocol Exception
		//
		// 패킷 파싱할때 발생하는 예외들
		//
		//////////////////////////////////////////////////////////////////////
		class ProtocolException : public IOException {
		public :
			ProtocolException () : IOException () {}
			ProtocolException ( std::string msg ) : IOException (msg) {}
			std::string getName () const { return "ProtocolException"; }
		};
	
			//////////////////////////////////////////////////////////////////////
			//
			// Idle Exception
			//
			// 일정 시간동안 peer 로부터 입력이 없는 경우
			//
			//////////////////////////////////////////////////////////////////////
			class IdleException : public ProtocolException {
			public :
				IdleException () : ProtocolException () {}
				IdleException ( std::string msg ) : ProtocolException (msg) {}
				std::string getName () const { return "IdleException"; }
			};
	

			//////////////////////////////////////////////////////////////////////
			//
			// Invalid Protocol Exception
			//
			// 잘못된 프로토콜
			//
			//////////////////////////////////////////////////////////////////////
			class InvalidProtocolException : public ProtocolException {
			public :
				InvalidProtocolException () : ProtocolException () {}
				InvalidProtocolException ( std::string msg ) : ProtocolException (msg) {}
				std::string getName () const { return "InvalidProtocolException"; }
			};
	
			//////////////////////////////////////////////////////////////////////
			//
			// Insufficient Data Exception
			//
			// 아직 패킷 데이타가 완전하게 도착하지 않았을 경우
			//
			//////////////////////////////////////////////////////////////////////
			class InsufficientDataException : public ProtocolException {
			public :
				InsufficientDataException ( uint size = 0 ) : ProtocolException () , m_Size(size) {}
				InsufficientDataException ( std::string msg , uint size = 0 ) : ProtocolException (msg) , m_Size(size) {}
				std::string getName () const { return "InsufficientDataException"; }
				uint getSize () const noexcept { return m_Size; }
				std::string toString () const
				{
					StringStream buf;
					buf << getName() << " : " << getMessage();
					if ( m_Size > 0 ) {
						buf << " ( lack of " << m_Size << " bytes )\n";
					}
					buf << getStackTrace ();
			
					return buf.toString();
				}
	
			private :
				uint m_Size;
			};

			//////////////////////////////////////////////////////////////////////
			// 
			// 프로토콜 예외, 시스템 예외 등으로인해서 접속을 짤라야 할 경우
			// 이 예외를 사용한다.
			// 
			//////////////////////////////////////////////////////////////////////
			class DisconnectException : public ProtocolException {
			public :
				DisconnectException () : ProtocolException () {}
				DisconnectException ( std::string msg ) : ProtocolException (msg) {}
				std::string getName () const { return "DisconnectException"; }
			};

			//////////////////////////////////////////////////////////////////////
			// 
			// 특정 상황때 무시해야 되는 패킷이 들어왔을 경우
			// 
			//////////////////////////////////////////////////////////////////////
			class IgnorePacketException : public ProtocolException {
			public :
				IgnorePacketException () : ProtocolException () {}
				IgnorePacketException ( std::string msg ) : ProtocolException (msg) {}
				std::string getName () const { return "IgnorePacketException"; }
			};


	//////////////////////////////////////////////////////////////////////
	//
	// Thread Exception
	//
	// 쓰레드 및 동기화 도구들에서 발생하는 예외들
	//
	//////////////////////////////////////////////////////////////////////
	class ThreadException : public Exception {
	public :
		ThreadException () : Exception () {}
		ThreadException ( std::string msg ) : Exception (msg) {}
		std::string getName () const { return "ThreadException"; }
	};

		//////////////////////////////////////////////////////////////////////
		//
		// Mutex Exception
		//
		// 뮤텍스에서 발생하는 예외들
		//
		//////////////////////////////////////////////////////////////////////
		class MutexException : public ThreadException {
		public :
			MutexException () : ThreadException () {}
			MutexException ( std::string msg ) : ThreadException (msg) {}
			std::string getName () const { return "MutexException"; }
		};

			//////////////////////////////////////////////////////////////////////
			//
			// Mutex Attribute Exception
			//
			// 뮤텍스 속성에서 발생하는 예외들
			//
			//////////////////////////////////////////////////////////////////////
			class MutexAttrException : public MutexException {
			public :
				MutexAttrException () : MutexException () {}
				MutexAttrException ( std::string msg ) : MutexException (msg) {}
				std::string getName () const { return "MutexAttrException"; }
			};


		//////////////////////////////////////////////////////////////////////
		//
		// Conditional Variable Exception
		//
		// Conditional Variable 에서 발생하는 예외 (이름이 너무 길다.. - -)
		//
		//////////////////////////////////////////////////////////////////////
		class CondVarException : public ThreadException {
		public :
			CondVarException () : ThreadException () {}
			CondVarException ( std::string msg ) : ThreadException (msg) {}
			std::string getName () const { return "CondVarException"; }
		};

		//////////////////////////////////////////////////////////////////////
		//
		// Semaphore Exception
		//
		// Semaphore 에서 발생하는 예외
		//
		//////////////////////////////////////////////////////////////////////
		class SemaphoreException : public ThreadException {
		public :
			SemaphoreException () : ThreadException () {}
			SemaphoreException ( std::string msg ) : ThreadException (msg) {}
			std::string getName () const { return "SemaphoreException"; }
		};


	//////////////////////////////////////////////////////////////////////
	//
	// SQL Exception 
	//
	// SQL 관련 예외
	//
	//////////////////////////////////////////////////////////////////////
	class SQLException : public Exception {
	public :
		SQLException () : Exception() {}
		SQLException ( std::string msg ) : Exception(msg) {}
		std::string getName () const { return "SQLException"; }
	};

		//////////////////////////////////////////////////////////////////////
		//
		// SQL Warning
		//
		// SQL 경고문을 나타내는 예외~~
		//
		//////////////////////////////////////////////////////////////////////
		class SQLWarning : public SQLException {
		public :
			SQLWarning () : SQLException() {}
			SQLWarning ( std::string msg ) : SQLException(msg) {}
			std::string getName () const { return "SQLWarning"; }
		};


		//////////////////////////////////////////////////////////////////////
		//
		// SQL Connect Exception
		//
		// SQL에 대한 연결 시도가 실패한 경우, 연결이 끊어졌을 경우 등
		//
		//////////////////////////////////////////////////////////////////////
		class SQLConnectException : public SQLException {
		public :
			SQLConnectException () : SQLException() {}
			SQLConnectException ( std::string msg ) : SQLException(msg) {}
			std::string getName () const { return "SQLConnectException"; }
		};

		//////////////////////////////////////////////////////////////////////
		//
		// Query Exception
		//
		//////////////////////////////////////////////////////////////////////
		class SQLQueryException : public SQLException {
		public :
			SQLQueryException () : SQLException() {}
			SQLQueryException ( std::string msg ) : SQLException(msg) {}
			std::string getName () const { return "SQLQueryException"; }
		};


	//////////////////////////////////////////////////////////////////////
	//
	// Runtime Exception
	//
	// 런타임에 발생가능한 generic 한 용도로 사용될 수 있는 예외들
	//
	//////////////////////////////////////////////////////////////////////
	class RuntimeException : public Exception {
	public :
		RuntimeException () : Exception () {}
		RuntimeException ( std::string msg ) : Exception (msg) {}
		std::string getName () const { return "RuntimeException"; }
	};
	
		//////////////////////////////////////////////////////////////////////
		//
		// Invalid Arguemnt Exception
		//
		// 함수, 멤버함수의 파라미터가 잘못된 경우 
		//
		//////////////////////////////////////////////////////////////////////
		class InvalidArgumentException : public RuntimeException {
		public :
			InvalidArgumentException () : RuntimeException () {}
			InvalidArgumentException ( std::string msg ) : RuntimeException (msg) {}
			std::string getName () const { return "InvalidArgumentException"; }
		};

		//////////////////////////////////////////////////////////////////////
		//
		// Out Of Bound Exception
		//
		// 말그대로. Out Of Bound!
		//
		//////////////////////////////////////////////////////////////////////
		class OutOfBoundException : public RuntimeException {
		public :
			OutOfBoundException () : RuntimeException () {}
			OutOfBoundException ( std::string msg ) : RuntimeException (msg) {}
			std::string getName () const { return "OutOfBoundException"; }
		};

		//////////////////////////////////////////////////////////////////////
		//
		// Interrupted Exception
		//
		// System Call 등이 인터럽트 당했을 경우
		//
		//////////////////////////////////////////////////////////////////////
		class InterruptedException : public RuntimeException {
		public :
			InterruptedException () : RuntimeException () {}
			InterruptedException ( std::string msg ) : RuntimeException (msg) {}
			std::string getName () const { return "InterruptedException"; }
		};

		//////////////////////////////////////////////////////////////////////
		//
		// No Such Element Exception
		//
		// 컬렉션에서 특정 키값을 검색했을때 그런 엘리먼트가 없는 경우
		//
		//////////////////////////////////////////////////////////////////////
		class NoSuchElementException : public RuntimeException {
		public :
			NoSuchElementException () : RuntimeException () {}
			NoSuchElementException ( std::string msg ) : RuntimeException (msg) {}
			std::string getName () const { return "NoSuchElementException"; }
		};

		//////////////////////////////////////////////////////////////////////
		//
		// Duplicated Exception
		//
		// 컬렉션의 특정 키가 중복되었을 때 
		//
		//////////////////////////////////////////////////////////////////////
		class DuplicatedException : public RuntimeException {
		public :
			DuplicatedException () : RuntimeException () {}
			DuplicatedException ( std::string msg ) : RuntimeException (msg) {}
			std::string getName () const { return "DuplicatedException"; }
		};

	//////////////////////////////////////////////////////////////////////
	//
	// Game Exception
	//
	// 게임에서 goto 용도로 사용하는 예외들.. -_-;
	//
	//////////////////////////////////////////////////////////////////////
	class GameException : public Exception {
	public :
		GameException () : Exception () {}
		GameException ( std::string msg ) : Exception (msg) {}
		std::string getName () const { return "GameException"; }
	};
	
		//////////////////////////////////////////////////////////////////////
		//
		// Portal Exception
		//
		// PC °¡ Æ÷Å»À» ¹â¾ÒÀ»¶§...Ä

		//
		//////////////////////////////////////////////////////////////////////
		class PortalException : public GameException {
		public :
			PortalException () : GameException () {}
			PortalException ( std::string msg ) : GameException (msg) {}
			std::string getName () const { return "PortalException"; }
		};


//////////////////////////////////////////////////////////////////////
//
// Error
//
//////////////////////////////////////////////////////////////////////
class Error : public Throwable {
public :
	Error () : Throwable() {}
	Error ( const std::string & msg ) : Throwable(msg) {}
	std::string getName () const { return "Error"; }
};	
	//////////////////////////////////////////////////////////////////////
	//
	// Game Error
	//
	//////////////////////////////////////////////////////////////////////
	class GameError : public Error {
	public :
		GameError () : Error () {}
		GameError ( std::string msg ) : Error(msg) {}
		std::string getName () const { return "GameError"; }
	};


	//////////////////////////////////////////////////////////////////////
	//
	// Assertion Error
	//
	//////////////////////////////////////////////////////////////////////
	class AssertionError : public Error {
	public :
		AssertionError () : Error () {}
		AssertionError ( std::string msg ) : Error(msg) {}
		std::string getName () const { return "AssertionError"; }
	};

	//////////////////////////////////////////////////////////////////////
	//
	// Unsupported Error
	//
	//////////////////////////////////////////////////////////////////////
	class UnsupportedError : public Error {
	public :
		UnsupportedError () : Error () {}
		UnsupportedError ( std::string msg ) : Error(msg) {}
		std::string getName () const { return "UnsupportedError"; }
	};

	//////////////////////////////////////////////////////////////////////
	//
	// Log Error
	//
	// 일반적인 에러와는 달리 LogError는 디폴트 로그파일에 로그될 수 없다.
	// (생각해보라. 로그매니저 자체의 에러를 어떻게 로그한다는 말인가?)
	//
	//////////////////////////////////////////////////////////////////////
	class LogError : public Error {
	public :
		LogError () : Error () {}
		LogError ( std::string msg ) : Error(msg) {}
		std::string getName () const { return "LogError"; }
	};

	//////////////////////////////////////////////////////////////////////
	//
	// Unknown Error
	//
	//////////////////////////////////////////////////////////////////////
	class UnknownError : public Error {
	public :
		UnknownError () : Error() {}
		UnknownError ( const std::string & msg ) : Error(msg) {}
		UnknownError ( const std::string & msg , uint ErrorCode ) : Error(msg), m_ErrorCode(ErrorCode) {}
		std::string getName () const { return "UnknownError"; }
		uint getErrorCode () const noexcept { return m_ErrorCode; }
		std::string toString () const
		{
			StringStream buf;
			buf << getName() << " : " << getMessage() << " ( " << getErrorCode() << " ) \n"
				<< getStackTrace () ;
			
			return buf.toString();
		}
	private :
		uint m_ErrorCode;
	};

// Assert macro for platforms that don't have it defined
#ifndef Assert
	#include <assert.h>
	#define Assert(cond) assert(cond)
#endif

#endif
