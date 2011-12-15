﻿/*
	Copyright (C) by Franksoft 2009 - 2011.

	This file is part of the YSLib project, and may only be used,
	modified, and distributed under the terms of the YSLib project
	license, LICENSE.TXT.  By continuing to use, modify, or distribute
	this file you indicate that you have read the license and
	understand and accept it fully.
*/

/*!	\defgroup Helper Helper
\ingroup YSLib
\brief YSLib 助手模块。
*/

/*!	\file yglobal.h
\ingroup Helper
\brief 平台相关的全局对象和函数定义。
\version r2467;
\author FrankHB<frankhb1989@gmail.com>
\since 早于 build 132 。
\par 创建时间:
	2009-12-22 15:14:57 +0800;
\par 修改时间:
	2011-12-11 21:36 +0800;
\par 字符集:
	UTF-8;
\par 模块名称:
	YSLib::Helper::YGlobal;
*/


#ifndef YSL_INC_HELPER_YGLOBAL_H_
#define YSL_INC_HELPER_YGLOBAL_H_

#include "../Core/ygdibase.h"
#include "../Core/yapp.h"
#include "../Core/ydevice.h"

YSL_BEGIN

/*!	\defgroup CustomGlobalConstants Custom Global Constants
\ingroup GlobalObjects
\brief 平台相关的全局常量。
\since build 173 。
*/
//@{
/*!
\brief 屏幕大小。
\since build 215 。
*/
const SDst MainScreenWidth(SCREEN_WIDTH), MainScreenHeight(SCREEN_HEIGHT);
//@}

/*!	\defgroup CustomGlobalVariables Custom Global Variables
\ingroup GlobalObjects
\brief 平台相关的全局变量。
\since build 173 。
*/
//@{
//@}


YSL_BEGIN_NAMESPACE(Devices)

/*!
\brief DS 屏幕。
\since 早于 build 218 。
*/
class DSScreen : public Screen
{
public:
	typedef int BGType;

private:
	BGType bg;

public:
	/*!
	\brief 构造：指定宽度和高度，从指定缓冲区指针。
	*/
	DSScreen(SDst, SDst, Drawing::BitmapPtr = nullptr);

	/*!
	\brief 复位。
	\note 无条件初始化。
	*/
	static void
	Reset();

	/*!
	\brief 取指针。
	\note 无异常抛出。
	\note 进行状态检查。
	*/
	virtual Drawing::BitmapPtr
	GetCheckedBufferPtr() const ynothrow;
	DefGetter(const ynothrow, const BGType&, BgID, bg)

	/*!
	\brief 更新。
	\note 复制到屏幕。
	*/
	void
	Update(Drawing::BitmapPtr);
	/*!
	\brief 更新。
	\note 以纯色填充屏幕。
	*/
	void
	Update(Drawing::Color = 0);
};

YSL_END_NAMESPACE(Devices)


/*!
\brief 平台相关的应用程序类。
\note 含默认接口。
\since build 215 。
*/
class DSApplication : public Application
{
	friend DSApplication&
	FetchGlobalInstance() ynothrow;

private:
	FontCache* pFontCache; //!< 默认字体缓存。
	shared_ptr<Devices::DSScreen> hScreenUp; //!< DS 上屏幕句柄。
	shared_ptr<Devices::DSScreen> hScreenDown; //!< DS 上屏幕句柄。
	shared_ptr<Desktop> hDesktopUp; //!< DS 下屏幕默认桌面句柄。
	shared_ptr<Desktop> hDesktopDown; //!< DS 下屏幕默认桌面句柄。

public:
	/*!
	\brief \c private 构造函数：非内联。
	\pre 断言检查：进程唯一性。
	*/
	DSApplication();

	/*!
	\brief 析构：释放资源。
	\since build 269 。
	*/
	virtual
	~DSApplication();

	/*!
	\brief 取字体缓存引用。
	\throw LoggedEvent 记录异常事件。
	\note 仅抛出以上异常。
	*/
	FontCache&
	GetFontCache() const ythrow(LoggedEvent);
	DefGetter(const ynothrow, const shared_ptr<Devices::DSScreen>&,
		ScreenUpHandle, hScreenUp)
	DefGetter(const ynothrow, const shared_ptr<Devices::DSScreen>&,
		ScreenDownHandle, hScreenDown)
	DefGetter(const ynothrow, const shared_ptr<Desktop>&, DesktopUpHandle,
		hDesktopUp)
	DefGetter(const ynothrow, const shared_ptr<Desktop>&, DesktopDownHandle,
		hDesktopDown)

	/*!
	\brief 取上屏幕。
	\note 断言检查：句柄非空。
	\note 无异常抛出。
	*/
	Devices::DSScreen&
	GetScreenUp() const ynothrow;
	/*!
	\brief 取下屏幕。
	\note 断言检查：句柄非空。
	\note 无异常抛出。
	*/
	Devices::DSScreen&
	GetScreenDown() const ynothrow;
	/*!
	\brief 取上桌面。
	\note 断言检查：句柄非空。
	\note 无异常抛出。
	*/
	Desktop&
	GetDesktopUp() const ynothrow;
	/*!
	\brief 取下桌面。
	\note 断言检查：句柄非空。
	\note 无异常抛出。
	*/
	Desktop&
	GetDesktopDown() const ynothrow;
	/*!
	\brief 取默认屏幕。
	\note 无异常抛出。
	*/
	PDefH(Devices::DSScreen&, GetDefaultScreen)
		ImplRet(GetScreenUp())
	/*!
	\brief 取默认桌面。
	\note 默认接口：可能会被 YSLib 本体使用。
	\note 无异常抛出。
	*/
	PDefH(Desktop&, GetDefaultDesktop)
		ImplRet(GetDesktopUp())
	/*!
	\brief 取触摸屏所在桌面。
	\note 默认接口：可能会被 YSLib 本体使用。
	\note 断言检查：句柄非空。
	\note 无异常抛出。
	*/
	PDefH(Desktop&, GetTouchableDesktop)
		ImplRet(GetDesktopDown())

	/*!
	\brief 复位默认字体缓存：使用指定路径。
	\throw LoggedEvent 记录异常事件。
	\note 仅抛出以上异常。
	*/
	void
	ResetFontCache(const_path_t) ythrow(LoggedEvent);
};

inline Devices::DSScreen&
DSApplication::GetScreenUp() const ynothrow
{
	YAssert(bool(hScreenUp), "Fatal error:"
		" null screen handle found @ DSApplication::GetScreenUp;");

	return *hScreenUp;
}
inline Devices::DSScreen&
DSApplication::GetScreenDown() const ynothrow
{
	YAssert(bool(hScreenDown), "Fatal error:"
		" null screen handle found @ DSApplication::GetScreenDown;");

	return *hScreenDown;
}
inline Desktop&
DSApplication::GetDesktopUp() const ynothrow
{
	YAssert(bool(hDesktopUp), "Fatal error:"
		" null desktop handle found @ DSApplication::GetDesktopUp;");

	return *hDesktopUp;
}
inline Desktop&
DSApplication::GetDesktopDown() const ynothrow
{
	YAssert(bool(hDesktopDown), "Fatal error:"
		" null desktop handle found @ DSApplication::GetDesktopDown;");

	return *hDesktopDown;
}


YSL_BEGIN_NAMESPACE(Messaging)

/*!
\brief 输入消息内容。
\since build 210 。
*/
class InputContent
{
public:
	KeysInfo Keys;
	Drawing::Point CursorLocation;

	explicit yconstfn
	InputContent(const KeysInfo&, const Drawing::Point&);

	bool
	operator==(const InputContent&) const;
};

inline yconstfn
InputContent::InputContent(const KeysInfo& k, const Drawing::Point& pt)
	: Keys(k), CursorLocation(pt)
{}

DefMessageTarget(SM_INPUT, shared_ptr<InputContent>)

YSL_END_NAMESPACE(Messaging)


//全局函数。

/*!
\brief 取平台相关的全局资源。
\note 无异常抛出。
\pre 内部断言检查存在应用程序实例。
\note 生存期未确定。需要手动初始化并注册应用程序实例后才能使用。
\since build 211 。
*/
DSApplication&
FetchGlobalInstance() ynothrow;

/*!
\brief 默认消息发生函数。
*/
void
Idle();

/*!
\brief 以指定前景色和背景色初始化指定屏幕的控制台。
\since build 148 。
*/
bool
InitConsole(Devices::Screen&, Drawing::PixelType, Drawing::PixelType);

/*!
\brief 显示致命错误。
\since build 175 。
*/
void
ShowFatalError(const char*);

/*!	\defgroup HelperFunctions Helper Functions
\brief 助手功能/函数。

仅帮助简化编码形式或确定接口，并不包含编译期之后逻辑功能实现的代码设施。
\since build 252 。
*/
//@{

/*!
\brief 取默认字体缓存。
\since build 219 。
*/
inline FontCache&
FetchDefaultFontCache()
{
	return FetchGlobalInstance().GetFontCache();
}

/*!
\brief 取默认屏幕。
\since build 219 。
*/
inline Devices::Screen&
FetchDefaultScreen()
{
	return FetchGlobalInstance().GetDefaultScreen();
}

//@}

YSL_END

#endif
