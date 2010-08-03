﻿// YSLib::Core::YShellMessage by Franksoft 2009 - 2010
// CodePage = UTF-8;
// CTime = 2009-12-6 2:44:31;
// UTime = 2010-8-2 14:58;
// Version = 0.1412;


#include "ysmsg.h"
#include "../Shell/ywindow.h" // for HWND delete procedure;

YSL_BEGIN

YSL_BEGIN_NAMESPACE(Shells)

const time_t DEF_TIMEOUT(0);

#ifdef YSLIB_NO_CURSOR

MMSG::MMSG(HSHL shl, MSGID m, MSGPRIORITY p, WPARAM w, const LPARAM l)
: hShl(shl), msg(m), prior(p), wParam(w), lParam(l), timestamp(std::clock()), timeout(DEF_TIMEOUT)
{
}

#else

MMSG::MMSG(HSHL shl, MSGID m, MSGPRIORITY p, WPARAM w, const LPARAM l, const SPoint& _pt)
: hShl(shl), msg(m), prior(p), wParam(w), lParam(l), pt(_pt), timestamp(std::clock()), timeout(DEF_TIMEOUT)
{
}

#endif

bool MMSG::operator==(const MMSG& m) const
{

#ifdef YSLIB_NO_CURSOR

	return hShl == m.hShl && msg == m.msg && prior == m.prior && wParam == m.wParam && lParam == m.lParam;

#else

	return hShl == m.hShl && msg == m.msg && prior == m.prior && wParam == m.wParam && lParam == m.lParam && pt == m.pt;

#endif

}


YMessageQueue::YMessageQueue()
: q()
{}
YMessageQueue::~YMessageQueue() ythrow()
{}

void
YMessageQueue::GetMessage(MMSG& m)
{
	if(!q.empty())
	{
		if(q.top().IsValid())
			m = q.top();
		q.pop();
	}
}
void
YMessageQueue::PeekMessage(MMSG& m) const
{
	if(!q.empty())
		if(q.top().IsValid())
			m = q.top();
}

YMessageQueue::size_type
YMessageQueue::Clear()
{
	size_type n = 0;

	while(!empty())
	{
		q.pop();
		++n;
	}
	return n;
}
void
YMessageQueue::Update()
{
	if(!q.empty())
	{
		if(q.top().IsTimeOut())
			q.pop();
	}
}
bool
YMessageQueue::InsertMessage(const MMSG& m)
{
	if(m.IsValid())
		q.push(m);
	return m.IsValid();
}

void
Merge(YMessageQueue& dst, std::vector<MMSG>& src)
{
	MMSG m;

	while(!src.empty())
	{
		m = src[src.size() - 1];
		dst.InsertMessage(m);
		src.pop_back();
	}
}
void
Merge(YMessageQueue& dst, YMessageQueue& src)
{
	MMSG m;

	while(!src.empty())
	{
		src.GetMessage(m);
		dst.InsertMessage(m);
	}
}

YSL_END_NAMESPACE(Shells)

YSL_END

