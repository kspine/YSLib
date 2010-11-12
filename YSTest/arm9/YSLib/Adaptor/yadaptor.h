﻿/*
	Copyright (C) by Franksoft 2010.

	This file is part of the YSLib project, and may only be used,
	modified, and distributed under the terms of the YSLib project
	license, LICENSE.TXT.  By continuing to use, modify, or distribute
	this file you indicate that you have read the license and
	understand and accept it fully.
*/

/*!	\defgroup Adaptor Adaptor
\ingroup YSLib
\brief YSLib 适配器模块。
*/

/*!	\file yadaptor.h
\ingroup Adaptor
\brief 外部库关联。
\version 0.1916;
\author FrankHB<frankhb1989@gmail.com>
\par 创建时间:
	2010-02-22 20:16:21 + 08:00;
\par 修改时间:
	2010-11-12 18:44 + 08:00;
\par 字符集:
	UTF-8;
\par 模块名称:
	YSLib::Adaptor::YAdaptor;
*/


#ifndef INCLUDED_YADAPTOR_H_
#define INCLUDED_YADAPTOR_H_

//包含编译配置。
#include "config.h"

//包含平台文件。
#include <platform.h>

//引入 YSLib 命名空间宏。
#include "base.h"


//包含 CHRLib 。

#include "../../CHRLib/chrlib.h"

namespace stdex
{
	using CHRLib::uchar_t;
	using CHRLib::uint_t;
}

YSL_BEGIN

YSL_BEGIN_NAMESPACE(Text)

	using namespace CHRLib;

YSL_END_NAMESPACE(Text)

	using Text::fchar_t;
	using Text::uchar_t;
	using Text::uint_t;

YSL_END

//包含 YCLib 。

#include "../../YCLib/ycommon.h"

// !\brief YSLib 命名空间。
YSL_BEGIN

// !\brief 图形处理。
YSL_BEGIN_NAMESPACE(Drawing)

	using platform::PixelType;
	using platform::BitmapPtr;
	using platform::ConstBitmapPtr;
	using platform::ScreenBufferType;
	using platform::Color;
	namespace ColorSpace = platform::ColorSpace;

YSL_END_NAMESPACE(Drawing)

//! \brief 运行时对象。 
YSL_BEGIN_NAMESPACE(Runtime)

	namespace KeySpace = platform::KeySpace;
	using platform::KeysInfo;
	using platform::CursorInfo;

YSL_END_NAMESPACE(Runtime)

	using platform::terminate;
	using platform::HDirectory;

	//! \brief 运行时平台。 
	namespace DS
	{
		using namespace platform;

		using ::swiWaitForVBlank;

		using ::lcdMainOnTop;
		using ::lcdMainOnBottom;
		using ::lcdSwap;
		using ::videoSetMode;
		using ::videoSetModeSub;

		using ::scanKeys;
		using ::touchRead;
	}

YSL_END


//包含 FreeType2 。

#include <ft2build.h>

#include FT_FREETYPE_H
#include FT_CACHE_H
#include FT_BITMAP_H
#include FT_GLYPH_H
//#include FT_OUTLINE_H


//包含 Anti-Grain Geometry 。
//#include "agg.h"


//包含 Loki 。
//#define LOKI_FUNCTOR_IS_NOT_A_SMALLOBJECT
#define LOKI_FUNCTORS_ARE_COMPARABLE //实现事件机制需要能被比较的仿函数。
#include <loki/Function.h>
#include <loki/SmartPtr.h>
#include <loki/StrongPtr.h>

YSL_BEGIN

//! \brief 基础语言设施。
YSL_BEGIN_NAMESPACE(Design)

	//类型操作和类型特征。
	using Loki::Typelist;
	namespace TL = Loki::TL;
	using Loki::NullType;
	using Loki::EmptyType;
	using Loki::Int2Type;
	using Loki::Type2Type;
	using Loki::Select;
	using Loki::IsCustomUnsignedInt;
	using Loki::IsCustomSignedInt;
	using Loki::IsCustomFloat;
	using Loki::TypeTraits;

	// Function 和 Function 。
	using Loki::Function;
	using Loki::Functor;

//! \brief 设计模式。
YSL_BEGIN_NAMESPACE(Pattern)
YSL_END_NAMESPACE(Pattern)

YSL_END_NAMESPACE(Design)

	//使用 Loki 智能指针。
	using Loki::SmartPtr;
	using Loki::RefCounted;
	using Loki::AllowConversion;
	using Loki::DisallowConversion;
	using Loki::RejectNull;
	using Loki::DefaultSPStorage;
	using Loki::HeapStorage;
	using Loki::RefToValue;
	using Loki::SmallObject;

YSL_END

//! \brief 使用 Loki 的编译期静态检查。
#define YSL_STATIC_CHECK(expr, msg) LOKI_STATIC_CHECK(expr, msg)

//! \brief 使用 Loki 的 Typelist 。
#define YSL_TL(n, ...) LOKI_TYPELIST##n(...)


#endif
