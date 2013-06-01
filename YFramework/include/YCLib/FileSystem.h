﻿/*
	Copyright by FrankHB 2012 - 2013.

	This file is part of the YSLib project, and may only be used,
	modified, and distributed under the terms of the YSLib project
	license, LICENSE.TXT.  By continuing to use, modify, or distribute
	this file you indicate that you have read the license and
	understand and accept it fully.
*/

/*!	\file FileSystem.h
\ingroup YCLib
\brief 平台相关的文件系统接口。
\version r668
\author FrankHB <frankhb1989@gmail.com>
\since build 312
\par 创建时间:
	2012-05-30 22:38:37 +0800
\par 修改时间:
	2013-06-01 19:23 +0800
\par 文本编码:
	UTF-8
\par 模块名称:
	YCLib::FileSystem
*/


#ifndef YCL_INC_FileSystem_h_
#define YCL_INC_FileSystem_h_ 1

#include "YCLib/ycommon.h"
#include <ystdex/utility.hpp> // for std::is_array, std::is_integral,
//	std::remove_reference, ystdex::arrlen;
#include <ystdex/cstring.h> // for ystdex::is_null;
#include <ystdex/string.hpp> // for ystdex::string_length;
#include <CHRLib/encoding.h>
#include <dirent.h>

namespace platform
{

//平台相关的全局常量。

//最大路径长度。
#ifdef PATH_MAX
#	define YCL_MAX_PATH_LENGTH PATH_MAX
#elif defined(MAXPATHLEN)
#	define YCL_MAX_PATH_LENGTH MAXPATHLEN
#else
#	define YCL_MAX_PATH_LENGTH 256
#endif

//最大文件名长度。
#ifdef NAME_MAX
#	define YCL_MAX_FILENAME_LENGTH NAME_MAX
#else
#	define YCL_MAX_FILENAME_LENGTH YCL_MAX_PATH_LENGTH
#endif

/*
\brief 判断字符串是否是当前路径。
\since build 409
*/
#define YCL_FS_StringIsCurrent(_s, _p) \
	(ystdex::string_length(_s) == 1 && _s[0] == yJOIN(_p, '.'))

/*
\brief 判断字符串是否是父目录。
\since build 409
*/
#define YCL_FS_StringIsParent(_s, _p) \
	(ystdex::string_length(_s) == 2 \
	&& _s[0] == yJOIN(_p, '.') && _s[1] == yJOIN(_p, '.'))

/*
\def YCL_FS_CharIsDelimiter
\brief 判断字符是否路径分隔符。
\since build 409
*/

/*
\def YCL_FS_StringIsRoot
\brief 判断字符是否表示根目录路径。
\since build 409
*/

#ifdef YCL_API_FILESYSTEM_POSIX
	/*!
	\brief 文件路径分隔符。
	\since build 298
	*/
#	define YCL_PATH_DELIMITER '/'
	/*!
	\brief 文件路径分界符。
	\since build 402
	*/
#	define YCL_PATH_SEPARATOR "/"
	/*!
	\brief 根目录路径。
	*/
#	define YCL_PATH_ROOT YCL_PATH_SEPARATOR

/*!
\brief 本机路径字符类型。
\since build 286
*/
typedef char NativePathCharType;

#	define YCL_FS_CharIsDelimiter(_c, _p) (_c == yjoin(_p, YCL_PATH_DELIMITER))
#	define YCL_FS_StringIsRoot(_s, _p) (platform_ex::FS_IsRoot(&_s[0]))

/*!
\brief 路径字符串编码。
\since build 402
*/
yconstexpr CHRLib::CharSet::Encoding CS_Path(CHRLib::CharSet::UTF_8);
#elif YCL_MINGW32
	/*!
	\brief 文件路径分隔符。
	\since build 296
	*/
#	define YCL_PATH_DELIMITER '\\'
//	#define YCL_PATH_DELIMITER L'\\'
	/*!
	\brief 文件路径分界符。
	\since build 402
	*/
#	define YCL_PATH_SEPARATOR "\\"
//	#define YCL_PATH_SEPARATOR L"\\"
	/*!
	\brief 虚拟根目录路径。
	\since build 297
	*/
#	define YCL_PATH_ROOT YCL_PATH_SEPARATOR

/*!
\brief 本机路径字符类型。
\since build 296
\todo 解决 const_path_t 冲突。
*/
//	typedef wchar_t NativePathCharType;
typedef char NativePathCharType;

#	define YCL_FS_CharIsDelimiter(_c, _p) \
	(_c == yJOIN(_p, '/') || _c == yJOIN(_p, '\\'))
#	define YCL_FS_StringIsRoot(_s, _p) \
		(ystdex::string_length(_s) == 3 \
		&& _s[1] == ':' && YCL_FS_CharIsDelimiter(_s[2], _p))

/*!
\brief 路径字符串编码。
\since build 402
*/
yconstexpr CHRLib::CharSet::Encoding CS_Path(CHRLib::CharSet::UTF_8);
#else
#	error Unsupported platform found!
#endif

//! \since build 402
//@{
static_assert(std::is_integral<decltype(YCL_PATH_DELIMITER)>::value,
	"Illegal type of delimiter found.");
static_assert(std::is_array<typename std::remove_reference<decltype(
	YCL_PATH_SEPARATOR)>::type>::value, "Non-array type of separator found.");
static_assert(ystdex::arrlen(YCL_PATH_SEPARATOR) == 2,
	"Wrong length of separator found.");
static_assert(YCL_PATH_SEPARATOR[0] == YCL_PATH_DELIMITER,
	"Mismatched path delimiter and separator found.");
static_assert(ystdex::is_null(YCL_PATH_SEPARATOR[1]),
	"Non-null-terminator as end of separator.");
//@}

//类型定义。
/*!
\brief 本机路径字符串类型。
\since build 286
*/
typedef NativePathCharType PATHSTR[YCL_MAX_PATH_LENGTH];
/*!
\brief 本机文件名类型。
\since build 286
*/
typedef NativePathCharType FILENAMESTR[YCL_MAX_FILENAME_LENGTH];

// using ystdex;
using ystdex::const_path_t;
using ystdex::path_t;


/*!
\brief 以 UTF-8 文件名无缓冲打开文件。
\param filename 文件名，意义同 POSIX <tt>::open</tt> 。
\param oflag 打开标识，基本语义同 POSIX 2003 ，具体行为取决于实现。
\pre 断言：<tt>filename</tt> 。
\bug MinGW32 环境下非线程安全。
\since build 324
*/
YF_API int
uopen(const char* filename, int oflag) ynothrow;
/*!
\brief 以 UTF-8 文件名无缓冲打开文件。
\param filename 文件名，意义同 POSIX <tt>::open</tt> 。
\param oflag 打开标识，基本语义同 POSIX 2003 ，具体行为取决于实现。
\param pmode 打开模式，基本语义同 POSIX 2003 ，具体行为取决于实现。
\pre 断言：<tt>filename</tt> 。
\bug MinGW32 环境下非线程安全。
\since build 324
*/
YF_API int
uopen(const char* filename, int oflag, int pmode) ynothrow;
/*!
\brief 以 UCS-2LE 文件名无缓冲打开文件。
\param filename 文件名，意义同 POSIX <tt>::open</tt> 。
\param oflag 打开标识，基本语义同 POSIX 2003 ，具体行为取决于实现。
\pre 断言：<tt>filename</tt> 。
\since build 324
*/
YF_API int
uopen(const char16_t* filename, int oflag) ynothrow;
/*!
\brief 以 UCS-2LE 文件名无缓冲打开文件。
\param filename 文件名，意义同 POSIX <tt>::open</tt> 。
\param oflag 打开标识，基本语义同 POSIX 2003 ，具体行为取决于实现。
\param pmode 打开模式，基本语义同 POSIX 2003 ，具体行为取决于实现。
\pre 断言：<tt>filename</tt> 。
\since build 324
*/
YF_API int
uopen(const char16_t* filename, int oflag, int pmode) ynothrow;

/*!
\brief 以 UTF-8 文件名打开文件。
\param filename 文件名，意义同 std::fopen 。
\param mode 打开模式，基本语义同 ISO C99 ，具体行为取决于实现。
\pre 断言：<tt>filename && mode && *mode != 0</tt> 。
\bug MinGW32 环境下非线程安全。
\since build 299
*/
YF_API std::FILE*
ufopen(const char* filename, const char* mode) ynothrow;
/*!
\brief 以 UCS-2LE 文件名打开文件。
\param filename 文件名，意义同 std::fopen 。
\param mode 打开模式，基本语义同 ISO C99 ，具体行为取决于实现。
\pre 断言：<tt>filename && mode && *mode != 0</tt> 。
\since build 324
*/
YF_API std::FILE*
ufopen(const char16_t* filename, const char16_t* mode) ynothrow;

/*!
\brief 判断指定 UTF-8 文件名的文件是否存在。
\note 使用 ufopen 实现。
\pre 断言：参数非空。
\since build 324
*/
YF_API bool
ufexists(const char*) ynothrow;
/*!
\brief 判断指定 UCS-2 文件名的文件是否存在。
\note 使用 ufopen 实现。
\pre 断言：参数非空。
\since build 324
*/
YF_API bool
ufexists(const char16_t*) ynothrow;
/*!
\brief 判断指定字符串为文件名的文件是否存在。
\note 使用 NTCTS 参数 ufexists 实现。
\since build 324
*/
template<class _tString>
inline bool
ufexists(const _tString& str) ynothrow
{
	return ufexists(str.c_str());
}

/*!
\brief 判断指定路径的目录是否存在。
\since build 324
*/
YF_API bool
direxists(const_path_t) ynothrow;

/*!
\brief 判断指定 UTF-8 路径的目录是否存在。
\bug MinGW32 环境下非线程安全。
\since build 324
*/
YF_API bool
udirexists(const_path_t) ynothrow;
/*!
\brief 判断指定字符串为目录名的目录是否存在。
\note 使用 NTCTS 参数 udirexists 实现。
\since build 326
*/
template<class _tString>
inline bool
udirexists(const _tString& str) ynothrow
{
	return udirexists(str.c_str());
}

/*!
\brief 当第一参数非空时取当前工作目录复制至指定缓冲区中。
\param buf 缓冲区起始指针。
\param size 缓冲区长。
\return 若成功为 buf ，否则为空指针。
\deprecated 特定平台上的编码不保证是 UTF-8 。
\since build 324
*/
YF_API char*
getcwd_n(char* buf, std::size_t size) ynothrow;

/*!
\brief 当第一参数非空时取当前工作目录（ UCS2-LE 编码）复制至指定缓冲区中。
\param buf 缓冲区起始指针。
\param size 缓冲区长。
\return 若成功为 buf ，否则为空指针。
\since build 324
*/
YF_API char16_t*
u16getcwd_n(char16_t* buf, std::size_t size) ynothrow;

/*!
\brief 切换当前工作路径至指定的 UTF-8 字符串。
\bug MinGW32 环境下非线程安全。
\since build 324
*/
YF_API int
uchdir(const_path_t) ynothrow;

/*!
\brief 按路径新建一个或多个目录。
\since build 324
*/
YF_API bool
mkdirs(const_path_t) ynothrow;

/*!
\brief 截断文件至指定长度。
\pre 指定文件需已经打开并可写。
\return 操作是否成功。
\note 不改变文件读写位置。
\since build 341

若文件不足指定长度，扩展并使用空字节填充；否则保留起始指定长度的字节。
*/
YF_API bool
truncate(std::FILE*, std::size_t) ynothrow;


/*!
\brief 文件系统节点迭代器。
\since build 298
*/
class YF_API HFileNode final
{
public:
#if YCL_DS
	typedef ::DIR* IteratorType; //!< 本机迭代器类型。
#else
	//! \since build 402
	typedef ::_WDIR* IteratorType; //!< 本机迭代器类型。
#endif

	/*!
	\brief 上一次操作结果，0 为无错误。
	\warning 不可重入。
	\warning 非线程安全。
	*/
	static int LastError;

private:
	IteratorType dir;

#if YCL_DS
	/*!
	\brief 节点信息。
	\since build 298
	*/
	::dirent* p_dirent;
#else
	/*!
	\brief 节点信息。
	\since build 402
	*/
	::_wdirent* p_dirent;

	/*!
	\brief 节点 UTF-8 名称。
	\since build 402
	*/
	mutable std::string utf8_name;
#endif

public:
	/*!
	\brief 构造：使用目录路径。
	\since build 319
	*/
	explicit
	HFileNode(const_path_t path = {}) ynothrow
		: dir(), p_dirent()
	{
		Open(path);
	}
	/*!
	\brief 复制构造：默认实现。
	\note 浅复制。
	*/
	HFileNode(const HFileNode&) = default;
	/*!
	\brief 析构。
	\since build 319
	*/
	~HFileNode() ynothrow
	{
		Close();
	}

	/*!
	\brief 复制赋值：默认实现。
	\note 浅复制。
	\since build 311
	*/
	HFileNode&
	operator=(const HFileNode&) = default;

	/*!
	\brief 迭代：向后遍历。
	\since build 319
	*/
	HFileNode&
	operator++() ynothrow;
	/*!
	\brief 迭代：向前遍历。
	\since build 319
	*/
	HFileNode
	operator++(int) ynothrow
	{
		return ++HFileNode(*this);
	}

	/*!
	\brief 判断文件系统节点有效性。
	\since build 319
	*/
	explicit
	operator bool() const ynothrow
	{
		return dir;
	}

	/*!
	\brief 从节点状态信息判断是否为目录。
	\since build 319
	*/
	bool
	IsDirectory() const ynothrow;

	/*!
	\brief 取节点名称。
	\return 非空结果。
	\since build 319
	*/
	const char*
	GetName() const ynothrow;

	/*!
	\brief 打开。
	\since build 319
	*/
	void
	Open(const_path_t) ynothrow;

	/*!
	\brief 关闭。
	\since build 319
	*/
	void
	Close() ynothrow;

	/*!
	\brief 复位。
	\since build 319
	*/
	void
	Reset() ynothrow;
};


/*!
\brief 判断指定路径字符串是否表示一个绝对路径。
\since build 152
*/
YF_API bool
IsAbsolute(const_path_t);

/*!
\brief 取指定路径的文件系统根节点名称的长度。
*/
YF_API std::size_t
GetRootNameLength(const_path_t);

} // namespace platform;

namespace platform_ex
{

#if YCL_DS
//! \since build 409
char16_t
FS_IsRoot(const char16_t*);
#endif

} // namespace platform_ex;

#endif

