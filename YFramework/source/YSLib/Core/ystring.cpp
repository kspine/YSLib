﻿/*
	Copyright (C) by Franksoft 2010 - 2012.

	This file is part of the YSLib project, and may only be used,
	modified, and distributed under the terms of the YSLib project
	license, LICENSE.TXT.  By continuing to use, modify, or distribute
	this file you indicate that you have read the license and
	understand and accept it fully.
*/

/*!	\file ystring.cpp
\ingroup Core
\brief 基础字符串管理。
\version r2704;
\author FrankHB<frankhb1989@gmail.com>
\par 创建时间:
	2010-03-05 22:44:04 +0800;
\par 修改时间:
	2012-01-31 16:48 +0800;
\par 文本编码:
	UTF-8;
\par 模块名称:
	YSLib::Core::YString;
*/


#include "YSLib/Core/ystring.h"

YSL_BEGIN

YSL_BEGIN_NAMESPACE(Text)

ucs2_t* String::s_str;


string
StringToMBCS(const ucs2string& s, const Encoding& cp)
{
	const auto t(static_cast<char*>(std::malloc((s.length() << 2) + 1)));

	if(t)
		UCS2ToMBCS(t, s.c_str(), cp);

	string str(t);

	std::free(t);
	return str;
}

YSL_END_NAMESPACE(Text)

YSL_END

