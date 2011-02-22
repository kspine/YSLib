﻿/*
	Copyright (C) by Franksoft 2011.

	This file is part of the YSLib project, and may only be used,
	modified, and distributed under the terms of the YSLib project
	license, LICENSE.TXT.  By continuing to use, modify, or distribute
	this file you indicate that you have read the license and
	understand and accept it fully.
*/

/*!	\file ylabel.cpp
\ingroup Shell
\brief 平台无关的图形用户界面部件实现。
\version 0.1855;
\author FrankHB<frankhb1989@gmail.com>
\par 创建时间:
	2011-01-22 08:32:34 + 08:00;
\par 修改时间:
	2011-02-20 15:21 + 08:00;
\par 字符集:
	UTF-8;
\par 模块名称:
	YSLib::Shell::YLabel;
*/


#include "ylabel.h"
#include "yuicont.h"
#include "ywindow.h"

YSL_BEGIN

YSL_BEGIN_NAMESPACE(Components)

YSL_BEGIN_NAMESPACE(Widgets)

MLabel::MLabel(const Drawing::Font& f)
	: Font(f), Margin(2, 2, 2, 2),
	AutoSize(true), AutoEllipsis(false), Text()
{}

void
MLabel::PaintText(Widget& w, const Graphics& g, const Point& pt)
{
	Drawing::TextState ts;

	ts.Font.SetFont(Font);
	ts.ResetForBounds(GetBoundsOf(w), g.GetSize(), Margin);
	ts.Color = w.ForeColor;
	DrawText(g, ts, Text);
}


YLabel::YLabel(const Rect& r, IUIBox* pCon, const Drawing::Font& f)
	: YWidget(r, pCon), MLabel(f)
{}

void
YLabel::DrawForeground()
{
	YWidgetAssert(this, Widgets::YLabel, DrawForeground);

	ParentType::DrawForeground();
	PaintText(*this, FetchContext(*this), LocateForWindow(*this));
}


MTextList::MTextList(const Drawing::Font& f)
	: MLabel(f)
{}

/*void
MTextList::PaintTextList(Widget& w, const Point& pt)
{
	IWindow* pWnd(FetchDirectWindowPtr(
		ystdex::polymorphic_crosscast<IWidget&>(w)));

	if(pWnd && wpTextRegion)
	{
		wpTextRegion->Font = Font;
		wpTextRegion->Font.Update();
		wpTextRegion->ResetPen();
		wpTextRegion->Color = w.ForeColor;
		wpTextRegion->SetSize(w.GetWidth(), w.GetHeight());
		SetMarginsTo(*wpTextRegion, 2, 2, 2, 2);
		DrawText(pWnd->GetContext(), pt);
		wpTextRegion->SetSize(0, 0);
	}
}*/

YSL_END_NAMESPACE(Widgets)

YSL_END_NAMESPACE(Components)

YSL_END

