﻿/*
	Copyright (C) by Franksoft 2010 - 2011.

	This file is part of the YSLib project, and may only be used,
	modified, and distributed under the terms of the YSLib project
	license, LICENSE.TXT.  By continuing to use, modify, or distribute
	this file you indicate that you have read the license and
	understand and accept it fully.
*/

/*!	\file ybutton.cpp
\ingroup Shell
\brief 样式相关的图形用户界面按钮控件实现。
\version 0.3450;
\author FrankHB<frankhb1989@gmail.com>
\par 创建时间:
	2010-10-04 21:23:32 +0800;
\par 修改时间:
	2011-03-07 22:10 +0800;
\par 字符集:
	UTF-8;
\par 模块名称:
	YSLib::Shell::Button;
*/


#include "button.h"
#include "yuicont.h"
#include "ywindow.h"
#include "ystyle.h"

YSL_BEGIN

YSL_BEGIN_NAMESPACE(Components)

YSL_BEGIN_NAMESPACE(Controls)

namespace
{
	template<u8 r, u8 g, u8 b>
	inline void
	transPixelEx(BitmapPtr dst)
	{
		*dst ^= Color(r, g, b) & ~BITALPHA;
	}

	void
	RectDrawButton(const Graphics& g, const Point& p, const Size& s,
		bool bPressed = false, Color c = Color(48, 216, 255))
	{
		YAssert(g.IsValid(), "err: @g is invalid.");

		FillRect(g, p, s, c);
		if(s.Width > 4 && s.Height > 4)
		{
			Size sz(s.Width - 4, (s.Height - 4) / 2);
			Point sp(p.X + 2, p.Y + 2);

			FillRect(g, sp, sz, Color(232, 240, 255));
			sp.Y += sz.Height;
			if(s.Height % 2 != 0)
				++sz.Height;
			FillRect(g, sp, sz, Color(192, 224, 255));
		}
		if(bPressed)
			TransformRect(g, p, s, transPixelEx<56, 24, 32>);
	}
}


YThumb::YThumb(const Rect& r, IUIBox* pCon)
	: YControl(r, pCon),
	MButton()
{
	FetchEvent<Enter>(*this) += &YThumb::OnEnter;
	FetchEvent<Leave>(*this) += &YThumb::OnLeave;
}

void
YThumb::DrawForeground()
{
	YWidgetAssert(this, Controls::YThumb, DrawForeground);

	ParentType::DrawForeground();

	IWindow* pWnd(FetchDirectWindowPtr(*this));

	RectDrawButton(pWnd->GetContext(), LocateForWindow(*this),
		GetSize(), bPressed);
	if(IsFocused())
		WndDrawFocus(pWnd, GetSize());
}

void
YThumb::OnEnter(InputEventArgs&)
{
	bPressed = true;
	Refresh();
}

void
YThumb::OnLeave(InputEventArgs&)
{
	bPressed = false;
	Refresh();
}


YButton::YButton(const Rect& r, IUIBox* pCon, const Drawing::Font& f)
	: YThumb(r, pCon),
	MLabel(f)
{}

void
YButton::DrawForeground()
{
	YWidgetAssert(this, Controls::YButton, DrawForeground);

	ParentType::DrawForeground();
	PaintText(*this, FetchContext(*this), LocateForWindow(*this));
}

YSL_END_NAMESPACE(Controls)

YSL_END_NAMESPACE(Components)

YSL_END

