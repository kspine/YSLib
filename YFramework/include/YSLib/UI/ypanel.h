﻿/*
	Copyright (C) by Franksoft 2011.

	This file is part of the YSLib project, and may only be used,
	modified, and distributed under the terms of the YSLib project
	license, LICENSE.TXT.  By continuing to use, modify, or distribute
	this file you indicate that you have read the license and
	understand and accept it fully.
*/

/*!	\file ypanel.h
\ingroup UI
\brief 样式无关的图形用户界面面板。
\version r1325;
\author FrankHB<frankhb1989@gmail.com>
\since build 201 。
\par 创建时间:
	2011-04-13 20:40:51 +0800;
\par 修改时间:
	2011-11-30 14:33 +0800;
\par 字符集:
	UTF-8;
\par 模块名称:
	YSLib::UI::YPanel;
*/


#ifndef YSL_INC_UI_YPANEL_H_
#define YSL_INC_UI_YPANEL_H_

#include "ycontrol.h"
#include "yuicont.h"
#include "../Service/yres.h"

YSL_BEGIN

YSL_BEGIN_NAMESPACE(Components)

/*!
\brief 面板。
\since build 201 。
*/
class Panel : public Control, protected MUIContainer
{
public:
	/*!
	\brief 构造：使用指定边界。
	*/
	explicit
	Panel(const Rect& = Rect::Empty);
	inline DefDeMoveCtor(Panel)

	virtual void
	operator+=(IWidget&);

	virtual bool
	operator-=(IWidget&);

	using MUIContainer::Contains;

	virtual PDefH(IWidget*, GetTopWidgetPtr, const Point& pt,
		bool(&f)(const IWidget&))
		ImplBodyBase(MUIContainer, GetTopWidgetPtr, pt, f)

	virtual Rect
	Refresh(const PaintContext&);
};

YSL_END_NAMESPACE(Components)

YSL_END

#endif
