﻿/*
	Copyright (C) by Franksoft 2009 - 2011.

	This file is part of the YSLib project, and may only be used,
	modified, and distributed under the terms of the YSLib project
	license, LICENSE.TXT.  By continuing to use, modify, or distribute
	this file you indicate that you have read the license and
	understand and accept it fully.
*/

/*!	\file main.cpp
\ingroup DS
\brief ARM7 主源文件。
\version 0.1026;
\author FrankHB<frankhb1989@gmail.com>
\par 创建时间:
	2009-12-18 12:27:40 +0800;
\par 修改时间:
	2011-03-05 17:05 +0800;
\par 字符集:
	UTF-8;
\par 模块名称:
	Platform::DS::Main;
*/


#include <nds.h>
//#include <dswifi7.h>
#include <maxmod7.h>

void VcountHandler()
{
	inputGetAndSend();
}

void VblankHandler(void)
{
	//Wifi_Update();
}


int main()
{
	irqInit();
	fifoInit();

	//从固件中读取用户设置。
	readUserSettings();

	// 开始使用 RTC 跟踪 IRQ 。
	initClockIRQ();

	SetYtrigger(80);

	//installWifiFIFO();
	installSoundFIFO();

	mmInstall(FIFO_MAXMOD);

	installSystemFIFO();

	irqSet(IRQ_VCOUNT, VcountHandler);
	irqSet(IRQ_VBLANK, VblankHandler);

	irqEnable(IRQ_VBLANK | IRQ_VCOUNT | IRQ_NETWORK);

	//保持 ARM7 空闲状态。
	while(true)
		swiWaitForVBlank();
}

