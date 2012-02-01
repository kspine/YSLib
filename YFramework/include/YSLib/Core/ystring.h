﻿/*
	Copyright (C) by Franksoft 2010 - 2012.

	This file is part of the YSLib project, and may only be used,
	modified, and distributed under the terms of the YSLib project
	license, LICENSE.TXT.  By continuing to use, modify, or distribute
	this file you indicate that you have read the license and
	understand and accept it fully.
*/

/*!	\file ystring.h
\ingroup Core
\brief 基础字符串管理。
\version r3031;
\author FrankHB<frankhb1989@gmail.com>
\since 早于 build 132 。
\par 创建时间:
	2010-03-05 22:06:05 +0800;
\par 修改时间:
	2012-01-31 17:01 +0800;
\par 文本编码:
	UTF-8;
\par 模块名称:
	YSLib::Core::YString;
*/


#ifndef YSL_INC_CORE_YSTRING_H_
#define YSL_INC_CORE_YSTRING_H_

#include "yobject.h"
#include "../Adaptor/ycont.h"

YSL_BEGIN

YSL_BEGIN_NAMESPACE(Text)

/*!
\brief YSLib 标准字符串（使用 UTF-16LE ）。
\since 早于 build 132 。
*/
class String : public ucs2string
{
private:
	static ucs2_t* s_str; //!< 内码转换生成字符串临时指针。

public:
	/*!
	\brief 无参数构造：默认实现。
	*/
	inline DefDeCtor(String)
	/*!
	\brief 复制构造：默认实现。
	*/
	inline DefDeCopyCtor(String)
	/*!
	\brief 转移构造：默认实现。
	*/
	inline DefDeMoveCtor(String)
	/*!
	\brief 构造：使用 UCS-2LE 字符指针表示的等宽 NTCTS 。
	*/
	String(const ucs2_t*);
	/*!
	\brief 构造：使用字符指针表示的 NTCTS 和指定编码。
	\since build 281 。
	*/
	template<typename _tChar>
	String(const _tChar*, Encoding = CP_Default);
	/*!
	\brief 构造：使用 YSLib 基本字符串。
	*/
	String(const ucs2string&);
	/*!
	\brief 构造：使用指定字符类型的 std::basic_string 和指定编码。
	\since build 281 。
	*/
	template<typename _tChar>
	String(const std::basic_string<_tChar>&, Encoding = CP_Default);
	inline DefDeDtor(String)
};

inline
String::String(const ucs2_t* s)
	: ucs2string(s)
{}
template<typename _tChar>
String::String(const _tChar* s, Encoding cp)
	: ucs2string(s_str = ucsdup(s, cp))
{
	std::free(s_str);
}
inline
String::String(const ucs2string& s)
	: ucs2string(s)
{}
template<typename _tChar>
String::String(const std::basic_string<_tChar>& s, Encoding cp)
	: ucs2string(s_str = ucsdup(s.c_str(), cp))
{
	std::free(s_str);
}


/*!
\brief YSLib 基本字符串转化为多字节字符串。
\since build 152 。
*/
string
StringToMBCS(const ucs2string&, const Encoding& = CP_Local);

YSL_END_NAMESPACE(Text)

YSL_END

#endif

