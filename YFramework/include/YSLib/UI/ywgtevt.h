﻿/*
	Copyright (C) by Franksoft 2010 - 2012.

	This file is part of the YSLib project, and may only be used,
	modified, and distributed under the terms of the YSLib project
	license, LICENSE.TXT.  By continuing to use, modify, or distribute
	this file you indicate that you have read the license and
	understand and accept it fully.
*/

/*!	\file ywgtevt.h
\ingroup UI
\brief 标准部件事件定义。
\version r2218;
\author FrankHB<frankhb1989@gmail.com>
\since build 241 。
\par 创建时间:
	2010-12-17 10:27:50 +0800;
\par 修改时间:
	2012-08-20 18:06 +0800;
\par 文本编码:
	UTF-8;
\par 模块名称:
	YSLib::UI::YWidgetEvent;
*/


#ifndef INCLUDED_YWGTEVT_H_
#define INCLUDED_YWGTEVT_H_

#include "ycomp.h"
#include "../Core/yevt.hpp"

YSL_BEGIN

YSL_BEGIN_NAMESPACE(Components)

/*!
\brief 用户界面绘制优先级。
\since build 294 。
*/
//@{
yconstexpr EventPriority BackgroundPriority(0xC0);
yconstexpr EventPriority BoundaryPriority(0x60);
yconstexpr EventPriority ForegroundPriority(0x40);
//@}


/*!
\brief 用户界面事件参数基类。
\warning 非虚析构。
\since build 255 。
*/
struct UIEventArgs
{
private:
	IWidget* pSender;

public:
	explicit
	UIEventArgs(IWidget& wgt)
		: pSender(&wgt)
	{}
	/*!
	\brief 复制构造：默认实现。
	\since build 295 。
	*/
	inline DefDeCopyCtor(UIEventArgs)

	/*!
	\brief 复制赋值：默认实现。
	\since build 295 。
	*/
	inline DefDeCopyAssignment(UIEventArgs)

	DefGetter(const ynothrow, IWidget&, Sender, *pSender)
	PDefH(void, SetSender, IWidget& wgt)
		ImplExpr(pSender = &wgt)
};


/*!
\brief 路由事件参数基类。
\warning 非虚析构。
\since build 195 。
*/
struct RoutedEventArgs : public UIEventArgs
{
public:
	//! 事件路由策略枚举。
	typedef enum
	{
		Bubble = 0, //!< 气泡事件：向上遍历视图树时触发。
		Tunnel = 1, //!< 隧道事件：向下遍历视图树时触发。
		Direct = 2 //!< 直接事件：仅当遍历至目标控件时触发。
	} RoutingStrategy;

	RoutingStrategy Strategy; //!< 事件路由策略。
	bool Handled; //!< 事件已经被处理。

	RoutedEventArgs(IWidget& wgt, RoutingStrategy strategy = Direct)
		: UIEventArgs(wgt),
		Strategy(strategy), Handled(false)
	{}
};


/*!
\brief 屏幕（指针设备）输入事件参数模块类。
\warning 非虚析构。
\since build 160 。
*/
typedef Drawing::Point MScreenPositionEventArgs;


/*!
\brief 输入事件参数类。
\warning 非虚析构。
\since 早于 build 132 。
*/
struct InputEventArgs : public RoutedEventArgs
{
public:
	/*!
	\brief 包含的按键代码。
	\since build 298 。
	*/
	KeyInput Keys;

	/*!
	\brief 构造：使用本机键按下对象和路由事件类型。
	\since build 298 。
	*/
	InputEventArgs(IWidget&, const KeyInput& = KeyInput(),
		RoutingStrategy = Direct);

	/*!
	\brief 转换为按键代码类型。
	\since build 298 。
	*/
	DefCvt(const ynothrow, const KeyInput&, Keys)

	/*!
	\brief 取按键代码。
	\since build 298 。
	*/
	DefGetter(const ynothrow, const KeyInput&, Keys, Keys)
};


/*!
\brief 按键输入事件参数类。
\warning 非虚析构。
\since 早于 build 132 。
*/
struct KeyEventArgs : public InputEventArgs
{
public:
	typedef KeyInput InputType; //!< 输入类型。

	/*!
	\brief 构造：使用输入类型对象和路由事件类型。
	*/
	KeyEventArgs(IWidget&, const InputType& = InputType(),
		RoutingStrategy = Direct);
};


/*!
\brief 指针设备输入事件参数类。
\warning 非虚析构。
\since 早于 build 132 。
*/
struct TouchEventArgs : public InputEventArgs, public MScreenPositionEventArgs
{
public:
	typedef Drawing::Point InputType; //!< 输入类型。

	/*!
	\brief 构造：使用输入类型对象和路由事件类型。
	*/
	TouchEventArgs(IWidget&, const InputType& = InputType::Zero,
		RoutingStrategy = Direct);
};


/*!
\brief 简单事件参数类。

保存部件引用和指定类型值的事件参数。
\warning 非虚析构。
\since build 268 。
*/
template<typename _type>
struct GValueEventArgs : public UIEventArgs
{
	//! \brief 值类型。
	typedef _type ValueType;

	ValueType Value;

	/*!
	\brief 构造：使用部件引用和值。
	*/
	template<typename... _tParams>
	yconstfn
	GValueEventArgs(IWidget& wgt, _tParams&&... args)
		: UIEventArgs(wgt),
		Value(yforward(args)...)
	{}
	DefCvt(const ynothrow, ValueType, Value)
};


/*
\brief 部件绘制上下文。
\warning 非虚析构。
\since build 255 。
*/
struct PaintContext
{
	Drawing::Graphics Target; //!< 渲染目标：图形接口上下文。
	Drawing::Point Location; //!< 相对渲染目标的偏移坐标，指定部件左上角的位置。
	Drawing::Rect ClipArea; //!< 相对于图形接口上下文的正则矩形，
		//指定需要保证被刷新的边界区域。

	inline DefDeCtor(PaintContext)
	PaintContext(const Drawing::Graphics& g, const Drawing::Point& pt,
		const Drawing::Rect& r)
		: Target(g), Location(pt), ClipArea(r)
	{}
};


/*!
\brief 部件绘制参数。
\warning 非虚析构。
\since build 242 。
*/
struct PaintEventArgs : public UIEventArgs, public PaintContext
{
	PaintEventArgs(IWidget&);
	PaintEventArgs(IWidget&, const PaintContext&);
	PaintEventArgs(IWidget&, const Drawing::Graphics&, const Drawing::Point&,
		const Drawing::Rect&);
};


//事件处理器类型。
DeclDelegate(HUIEvent, UIEventArgs&&)
DeclDelegate(HInputEvent, InputEventArgs&&)
DeclDelegate(HKeyEvent, KeyEventArgs&&)
DeclDelegate(HTouchEvent, TouchEventArgs&&)
DeclDelegate(HPaintEvent, PaintEventArgs&&)
//DefDelegate(HPointEvent, Drawing::Point&&)
//DefDelegate(HSizeEvent, Size&&)


#define DefEventTypeMapping(_name, _tEventHandler) \
	template<> \
	struct EventTypeMapping<_name> \
	{ \
		typedef _tEventHandler HandlerType; \
	};


/*!
\brief 标准控件事件空间。
\since build 192 。
*/
typedef enum
{
//	AutoSizeChanged,
//	BackColorChanged,
//	ForeColorChanged,
//	LocationChanged,
//	MarginChanged,
//	VisibleChanged,

//	EnabledChanged,

	//视图变更事件。
	Move, //!< 移动：位置调整。
	Resize, //!< 大小调整。

	//图形用户界面输入事件。
	KeyUp, //!< 键接触结束。
	KeyDown, //!< 键接触开始。
	KeyHeld, //!< 键接触保持。
	KeyPress, //!< 键按下。
	TouchUp, //!< 屏幕接触结束。
	TouchDown, //!< 屏幕接触开始。
	TouchHeld, //!< 屏幕接触保持。
	TouchMove, //!< 屏幕接触移动。
	Click, //!< 屏幕点击。

	//图形用户界面输出事件。
	Paint, //!< 界面绘制。

	//焦点事件。
	GotFocus, //!< 焦点获得。
	LostFocus, //!< 焦点失去。

	//边界事件。
	Enter, //!< 控件进入。
	Leave //!< 控件离开。
//	TextChanged,
//	FontChanged,
//	FontColorChanged,
//	FontSizeChanged,
} VisualEvent;


template<VisualEvent>
struct EventTypeMapping
{
	//定义 HandlerType 的默认值可能会导致运行期 dynamic_cast 失败。
//	typedef HEvent HandlerType;
};

DefEventTypeMapping(Move, HUIEvent)
DefEventTypeMapping(Resize, HUIEvent)

DefEventTypeMapping(KeyUp, HKeyEvent)
DefEventTypeMapping(KeyDown, HKeyEvent)
DefEventTypeMapping(KeyHeld, HKeyEvent)
DefEventTypeMapping(KeyPress, HKeyEvent)
DefEventTypeMapping(TouchUp, HTouchEvent)
DefEventTypeMapping(TouchDown, HTouchEvent)
DefEventTypeMapping(TouchHeld, HTouchEvent)
DefEventTypeMapping(TouchMove, HTouchEvent)
DefEventTypeMapping(Click, HTouchEvent)

DefEventTypeMapping(Paint, HPaintEvent)

DefEventTypeMapping(GotFocus, HUIEvent)
DefEventTypeMapping(LostFocus, HUIEvent)

DefEventTypeMapping(Enter, HTouchEvent)
DefEventTypeMapping(Leave, HTouchEvent)


/*!
\brief 事件映射命名空间。
\since build 242 。
*/
YSL_BEGIN_NAMESPACE(EventMapping)

typedef GEventPointerWrapper<UIEventArgs&&> MappedType; //!< 映射项类型。
typedef GIHEvent<UIEventArgs&&> ItemType;
typedef pair<VisualEvent, MappedType> PairType;
typedef map<VisualEvent, MappedType> MapType; //!< 映射表类型。
typedef pair<typename MapType::iterator, bool> SearchResult; \
	//!< 搜索表结果类型。

YSL_END_NAMESPACE(EventMapping)

typedef EventMapping::MapType VisualEventMap;


//! \brief 错误或不存在的部件事件异常。
struct BadEvent
{};


/*!
\brief 控制器抽象类。
\since build 243 。
*/
class AController
{
private:
	bool enabled; //!< 控件可用性。

public:
	/*!
	\brief 构造：使用指定可用性。
	*/
	AController(bool b = true)
		: enabled(b)
	{}
	/*!
	\brief 析构：空实现。
	\since build 295 。
	*/
	virtual DefEmptyDtor(AController)

	DefPred(const ynothrow, Enabled, enabled)

	/*!
	\brief 取事件项。
	\since build 293 。
	*/
	DeclIEntry(EventMapping::ItemType& GetItem(const VisualEvent&))
	/*!
	\brief 取事件项，若不存在则用指定函数指针添加。
	\throw 忽略加入任何事件项。
	*/
	virtual EventMapping::ItemType&
	GetItemRef(const VisualEvent& id, EventMapping::MappedType(&)())
	{
		return GetItem(id);
	}

	DefSetter(bool, Enabled, enabled)

	/*
	\brief 复制实例。
	\note 应为抽象方法，但无法使用协变返回类型，所以使用非抽象实现。
	\warning 断言：禁止直接使用。
	*/
	virtual AController*
	Clone()
	{
		YAssert(false, "Invalid call.");

		return nullptr;
	}
};


template<class _tEventHandler>
size_t
DoEvent(AController& controller, const VisualEvent& id,
	typename _tEventHandler::EventArgsType&& e)
{
	try
	{
		return dynamic_cast<EventT(typename _tEventHandler)&>(
			controller.GetItem(id))(std::move(e));
	}
	catch(std::out_of_range&)
	{}
	catch(std::bad_cast&)
	{}
	return 0;
}
template<class _tEventHandler>
inline size_t
DoEvent(AController& controller, const VisualEvent& id,
	typename _tEventHandler::EventArgsType& e)
{
	return DoEvent<_tEventHandler>(controller, id, std::move(e));
}


/*!
\brief 部件控制器。
\since build 236 。
*/
class WidgetController : public AController
{
public:
	//! \since build 331 。
	GEventWrapper<EventT(HPaintEvent), UIEventArgs&&> Paint;

	/*!
	\brief 构造：使用指定可用性。
	*/
	explicit
	WidgetController(bool = false);

	ImplI(AController) EventMapping::ItemType&
	GetItem(const VisualEvent&);

	ImplI(AController) DefClone(WidgetController, Clone)
};

YSL_END_NAMESPACE(Components)

YSL_END

#endif

