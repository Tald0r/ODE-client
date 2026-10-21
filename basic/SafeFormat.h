//----------------------------------------------------------------------
// SafeFormat.h
//----------------------------------------------------------------------
//
// A printf that treats its format string as data.
//
// Every UI string in this client comes from Data/Info/String.inf and is
// handed to sprintf as the *format* argument in every place that has
// not yet been converted to this - ratchet R7 in
// tests/ratchet/ratchets.sh is the current count, so that this comment
// cannot go stale (docs/reviews/code-health-review-2026-08-29.md finding C19,
// docs/reviews/RESTRUCTURING.md task 5.4). printf's contract is that the format
// and the argument list agree, and on those call sites they cannot: the
// format is read off disk and the arguments are fixed in the source, so
// an entry carrying one %s more than the call site passes makes the CRT
// read a stack word as a char* and copy it, unbounded, into a fifty byte
// buffer.
//
// SafeFormat checks the two against each other at the point of use, which
// is the only place both are known. A conversion consumes the next
// argument only if that argument's type can satisfy it. A conversion with
// no argument left, with an argument of the wrong type, a %n, or a width
// or precision taken from an argument is not performed at all: the
// specification is copied out literally, so the operator sees "%s" in the
// message and the program reads nothing it was not handed. The
// destination is bounded by its own size and always terminated.
//
// This is the use-time half of the defence. The load-time half is
// SanitizeGameStringTable() in Client/MGameStringTable.cpp, which scrubs
// String.inf as it is read; it cannot check arity, because a table does
// not know its call sites, and it does not run in the default English
// build at all. SafeFormat has the arguments, so it can - but it still
// cannot check that an entry *means* what the call site passes, only that
// reading it is safe. A mistranslated entry prints wrongly; it no longer
// prints something the caller never owned.
//
//----------------------------------------------------------------------

#ifndef __SAFE_FORMAT_H__
#define __SAFE_FORMAT_H__

#include <stddef.h>
#include <string>
#include <type_traits>

namespace SafeFormat {

//----------------------------------------------------------------------
// One argument, tagged with what it actually is.
//
// nBytes is the width of the value as the caller declared it, and it is
// not cosmetic: printf("%x", -1) prints ffffffff for an int and sixteen f
// for a long long, so the conversion has to be issued at the original
// width or the output changes.
//----------------------------------------------------------------------
enum ArgKind
{
	ARG_NONE = 0,
	ARG_SIGNED,
	ARG_UNSIGNED,
	ARG_DOUBLE,
	ARG_STRING,
	ARG_POINTER
};

struct Arg
{
	ArgKind			kind;
	unsigned char	nBytes;

	union
	{
		long long			sValue;
		unsigned long long	uValue;
		double				dValue;
		const char*			pString;
		const void*			pPointer;
	};

	Arg() : kind(ARG_NONE), nBytes(0) { sValue = 0; }
};

//----------------------------------------------------------------------
// The whole implementation. The templates below only pack arguments;
// nothing about the format string is decided in a header.
//
// Returns the number of bytes written, not counting the terminator, so a
// truncated result reports what really fits rather than what was asked
// for - the opposite of snprintf, and the number a caller that appends
// needs.
//----------------------------------------------------------------------
int FormatV(char* pDest, size_t nSize, const char* pFormat,
			const Arg* pArgs, size_t nCount);

//----------------------------------------------------------------------
// Packing. An unsupported type is a compile error at the call site, which
// is the point: a caller passing something this does not understand
// should say what it means rather than have it silently dropped.
//----------------------------------------------------------------------
template <typename T>
inline typename std::enable_if<std::is_enum<T>::value, Arg>::type
MakeArg(T value)
{
	Arg a;
	a.kind = ARG_SIGNED;
	a.nBytes = (unsigned char)sizeof(int);
	a.sValue = (long long)value;
	return a;
}

template <typename T>
inline typename std::enable_if<std::is_integral<T>::value
							&& std::is_signed<T>::value, Arg>::type
MakeArg(T value)
{
	Arg a;
	a.kind = ARG_SIGNED;
	a.nBytes = (unsigned char)sizeof(T);
	a.sValue = (long long)value;
	return a;
}

template <typename T>
inline typename std::enable_if<std::is_integral<T>::value
							&& !std::is_signed<T>::value, Arg>::type
MakeArg(T value)
{
	Arg a;
	a.kind = ARG_UNSIGNED;
	a.nBytes = (unsigned char)sizeof(T);
	a.uValue = (unsigned long long)value;
	return a;
}

template <typename T>
inline typename std::enable_if<std::is_floating_point<T>::value, Arg>::type
MakeArg(T value)
{
	Arg a;
	a.kind = ARG_DOUBLE;
	a.nBytes = (unsigned char)sizeof(double);
	a.dValue = (double)value;
	return a;
}

inline Arg MakeArg(const char* pValue)
{
	Arg a;
	a.kind = ARG_STRING;
	a.nBytes = (unsigned char)sizeof(const char*);
	a.pString = pValue;
	return a;
}

inline Arg MakeArg(char* pValue)
{
	return MakeArg((const char*)pValue);
}

//----------------------------------------------------------------------
// A std::string argument borrows its buffer. That is safe even for a
// temporary: the temporary lives to the end of the full expression, and
// the Format call it was written into completes inside that expression.
//----------------------------------------------------------------------
inline Arg MakeArg(const std::string& value)
{
	return MakeArg(value.c_str());
}

template <typename T>
inline typename std::enable_if<std::is_pointer<T>::value, Arg>::type
MakeArg(T value)
{
	Arg a;
	a.kind = ARG_POINTER;
	a.nBytes = (unsigned char)sizeof(const void*);
	a.pPointer = (const void*)value;
	return a;
}

//----------------------------------------------------------------------
// Format into a destination whose size the caller states.
//----------------------------------------------------------------------
template <typename ...Args>
inline int Format(char* pDest, size_t nSize, const char* pFormat, Args... args)
{
	// The trailing default keeps this a legal array when the pack is
	// empty; nCount, not the array length, is what FormatV reads.
	const Arg packed[] = { MakeArg(args)..., Arg() };

	return FormatV(pDest, nSize, pFormat, packed, sizeof...(args));
}

//----------------------------------------------------------------------
// Format into a real array, whose size the compiler states.
//
// This overload is the one to reach for. sprintf's worst habit at these
// call sites is not the format string but the missing bound, and a
// destination that is genuinely an array cannot get that bound wrong.
// A char* destination has to use the overload above and say the size.
//----------------------------------------------------------------------
template <size_t N, typename ...Args>
inline int Format(char (&pDest)[N], const char* pFormat, Args... args)
{
	return Format(pDest, N, pFormat, args...);
}

// Copy and append plain text without interpreting percent signs. Both return
// the destination length actually stored and terminate any nonempty buffer.
// Overlapping source/destination ranges are supported. The source must be a
// valid C string or contain at least the number of bytes that can fit.
// Append bounds its destination scan, repairing an unterminated buffer by
// keeping its first capacity - 1 bytes. A null source is empty text.
size_t Copy(char* destination, size_t capacity, const char* source);
size_t Append(char* destination, size_t capacity, const char* source);

template <size_t N>
inline size_t Copy(char (&destination)[N], const char* source)
{
	return Copy(destination, N, source);
}

template <size_t N>
inline size_t Append(char (&destination)[N], const char* source)
{
	return Append(destination, N, source);
}

} // namespace SafeFormat

#endif // __SAFE_FORMAT_H__
