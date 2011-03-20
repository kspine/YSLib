﻿/*
	Copyright (C) by Franksoft 2010 - 2011.

	This file is part of the YSLib project, and may only be used,
	modified, and distributed under the terms of the YSLib project
	license, LICENSE.TXT.  By continuing to use, modify, or distribute
	this file you indicate that you have read the license and
	understand and accept it fully.
*/

/*!	\defgroup Service Service
\ingroup YSLib
\brief YSLib 服务模块。
*/

/*!	\file ytmgr.h
\ingroup Service
\brief 文本管理服务。
\version 0.4336;
\author FrankHB<frankhb1989@gmail.com>
\par 创建时间:
	2010-01-05 17:48:09 +0800;
\par 修改时间:
	2011-03-05 17:05 +0800;
\par 字符集:
	UTF-8;
\par 模块名称:
	YSLib::Service::YTextManager;
*/


#ifndef INCLUDED_YTMGR_H_
#define INCLUDED_YTMGR_H_

#include "../Core/ysdef.h"
#include "../Core/yobject.h"
#include "../Core/ystring.h"
#include "../Core/yftext.h"
#include "../Adaptor/cont.h"

YSL_BEGIN

YSL_BEGIN_NAMESPACE(Text)

//文本缓冲区。
class TextBuffer : private NonCopyable
{
private:
	const SizeType capacity; //!< 最大长度（字符数）。

protected:
	//文本缓冲区的首地址和长度。
	uchar_t* const text;
	SizeType len;

	/*!
	\brief 构造：使用指定最大长度。
	\note 保护实现。
	*/
	explicit
	TextBuffer(SizeType);
	/*!
	\brief 析构：释放资源。
	\note 保护实现。
	*/
	~TextBuffer();

public:
	/*!
	\brief 返回指定下标的字符。
	\note 断言检查。
	\note 无运行期范围检查。
	*/
	uchar_t&
	operator[](SizeType) ythrow();

	DefGetter(SizeType, Capacity, capacity) //!< 取最大文本长度。
	DefGetter(SizeType, SizeOfBuffer, sizeof(uchar_t) * capacity) //!< 取文本缓冲区的大小。
	DefGetter(uchar_t*, Ptr, text) //!< 取文本缓冲区的指针。
	DefGetter(SizeType, Length, len) //!< 取文本缓冲区的长度。
	/*!
	\brief 文本缓冲区下标 o （不含）起逆序查找字符 c 。
	\note 从返回结果的直接后继下标；查找失败时返回 0 。
	*/
	SizeType
	GetPrevChar(SizeType o, uchar_t c);
	/*!
	\brief 从文本缓冲区下标 o （含）起顺序查找字符 c 。
	\note 返回结果的下标：
	\li 查找失败时返回缓冲区长度（o 原值小于缓冲区长度）；
	\li 或 o 原值（大于等于缓冲区长度时）。
	*/
	SizeType
	GetNextChar(SizeType o, uchar_t c);
	/*!
	\brief 从文本缓冲区下标 o （不含）起逆序查找换行符。
	\note 返回结果的直接后继下标；查找失败时返回 0 。
	*/
	SizeType
	GetPrevNewline(SizeType o);
	/*!
	\brief 从文本缓冲区下标 o （含）起顺序查找字符 c 。
	\note 返回结果的下标；
	\li 查找失败时返回缓冲区长度（o 原值小于缓冲区长度）；
	\li 或 o 原值（大于等于缓冲区长度时）。
	*/
	SizeType
	GetNextNewline(SizeType o);

	/*!
ythrow(std::out_of_range)
	\brief 返回指定下标的字符。
	\note 当越界时抛出 std::out_of_range 异常。
	*/
	uchar_t&
	at(SizeType) ythrow(std::out_of_range);

	/*!
	\brief 清空缓冲区。
	*/
	void
	ClearText();

	/*!
	\brief 从起始地址中读取连续的 capacity 个 uchar_t 字符。
	*/
	bool
	Load(const uchar_t* s);
	/*!
	\brief 从起始地址中读取连续的 n 个 uchar_t 字符。
	\note 超过最大长度则放弃读取。
	*/
	bool
	Load(const uchar_t* s, SizeType n);
	/*!
	\brief 从文本文件 f 中读取连续的 capacity 个字符，
		并返回成功读取的字符数。
	\note 自动校验换行并转换为 Unix / Linux 格式。
	*/
	SizeType
	Load(YTextFile& f);
	/*!
	\brief 从文本文件 f 中读取连续的 n 个（）uchar_t 字符，
		并返回成功读取的字符数。
	\note 超过最大长度则放弃读取。
	\note 自动校验换行并转换为 Unix / Linux 格式。
	*/
	SizeType
	Load(YTextFile& f, SizeType n);

	/*!
	\brief 从文本文件 f 中读取连续的 n 个字节，并返回成功读取的字符数。
	*/
	SizeType
	LoadN(YTextFile& f, SizeType n);

	/*!
	\brief 从偏移 p 个字符起输出 n 个 uchar_t 字符到 d 。
	*/
	bool
	Output(uchar_t* d, SizeType p, SizeType n) const;
};

inline
TextBuffer::~TextBuffer()
{
	ydelete_array(text);
}

inline SizeType
TextBuffer::GetPrevNewline(SizeType o)
{
	return GetPrevChar(o, '\n');
}
inline SizeType
TextBuffer::GetNextNewline(SizeType o)
{
	return GetNextChar(o, '\n');
}

inline void
TextBuffer::ClearText()
{
	mmbset(text, 0, GetSizeOfBuffer());
}
inline bool
TextBuffer::Load(const uchar_t* s)
{
	return Load(s, capacity);
}
inline SizeType
TextBuffer::Load(YTextFile& f)
{
	return Load(f, capacity);
}


//文本缓冲区块。
class TextBlock : public TextBuffer
{
public:
	typedef u16 BlockSizeType;

	BlockSizeType Index;

	/*!
	\brief 构造：使用区块的大小和总长度。
	*/
	TextBlock(BlockSizeType, SizeType);
	/*!
	\brief 析构。
	\note 空实现。
	*/
	virtual
	~TextBlock();
};

inline
TextBlock::TextBlock(BlockSizeType i, SizeType tlen)
	: TextBuffer(tlen),
	Index(i)
{}
inline
TextBlock::~TextBlock()
{}


//文本缓冲区块映射。
class TextMap
{
public:
	typedef TextBlock::BlockSizeType BlockSizeType;
	typedef map<BlockSizeType, TextBlock*> MapType;

protected:
	MapType Map;

public:
	/*!
	\brief 析构。
	\note 清理文本区块和映射。
	*/
	virtual
	~TextMap();

	/*!
	\brief 取指定区块号的文本区块。
	*/
	TextBlock*
	operator[](const BlockSizeType&);
	/*!
	\brief 添加文本区块。
	*/
	void
	operator+=(TextBlock&);
	/*!
	\brief 移除指定区块号的文本区块。
	*/
	bool
	operator-=(const BlockSizeType&);

	/*!
	\brief 清理文本区块和映射。
	*/
	void
	Clear();
};

inline
TextMap::~TextMap()
{
	Clear();
}

inline TextBlock*
TextMap::operator[](const BlockSizeType& id)
{
	return Map[id];
}

inline void
TextMap::operator+=(TextBlock& item)
{
	Map.insert(std::make_pair(item.Index, &item));
}
inline bool
TextMap::operator-=(const BlockSizeType& i)
{
	return Map.erase(i) != 0;
}


//文本文件块缓冲区。
class TextFileBuffer : public TextMap
{
public:
	typedef TextMap::BlockSizeType BlockSizeType; //!< 区块索引类型。

	static const SizeType nBlockSize = 0x2000; //!< 文本区块容量。

	//只读文本循环迭代器类。
	class HText
	{
		friend class TextFileBuffer;

	public:
		typedef TextFileBuffer ContainerType;
		typedef ContainerType::BlockSizeType BlockSizeType;

	private:
		TextFileBuffer* pBuffer;
		//文本读取位置。
		BlockSizeType blk;
		SizeType idx;

	public:
		/*!
		\brief 构造：指定文本读取位置。
		\note 无异常抛出。
		*/
		HText(TextFileBuffer* = NULL, BlockSizeType = 0, SizeType = 0) ythrow();

		/*!
		\brief 迭代：循环向后遍历。
		\note 无异常抛出。
		*/
		HText&
		operator++() ythrow();

		/*!
		\brief 迭代：循环向前遍历。
		\note 无异常抛出。
		*/
		HText&
		operator--() ythrow();

		uchar_t
		operator*() ythrow();

		/*!
		\brief 迭代：重复循环向后遍历。
		\note 构造新迭代器进行迭代并返回。
		*/
		HText
		operator+(std::ptrdiff_t);

		/*!
		\brief 迭代：重复循环向前遍历。
		\note 构造新迭代器进行迭代并返回。
		*/
		HText
		operator-(std::ptrdiff_t);

		/*!
		\brief 比较：相等关系。
		*/
		friend bool
		operator==(const HText&, const HText&) ythrow();

		/*!
		\brief 比较：不等关系。
		*/
		friend bool
		operator!=(const HText&, const HText&) ythrow();

		/*!
		\brief 比较：严格偏序递增关系。
		*/
		friend bool
		operator<(const HText&, const HText&) ythrow();

		/*!
		\brief 比较：严格偏序递减关系。
		*/
		friend bool
		operator>(const HText&, const HText&) ythrow();

		/*!
		\brief 比较：非严格偏序递增关系。
		*/
		friend bool
		operator<=(const HText&, const HText&) ythrow();

		/*!
		\brief 比较：非严格偏序递减关系。
		*/
		friend bool
		operator>=(const HText&, const HText&) ythrow();

		/*!
		\brief 迭代：重复循环向后遍历。
		*/
		HText&
		operator+=(std::ptrdiff_t);

		/*!
		\brief 迭代：重复循环向前遍历。
		*/
		HText&
		operator-=(std::ptrdiff_t);

		DefGetter(TextFileBuffer*, BufferPtr, pBuffer)
		DefGetter(BlockSizeType, BlockN, blk)
		/*!
		\brief 取文字缓冲区指针。
		\note 无异常抛出。
		*/
		DefGetter(SizeType, IndexN, idx)
		const uchar_t*
		GetTextPtr() const ythrow();
		/*!
		\brief 取当前区块的长度。
		\note 无异常抛出。
		*/
		SizeType
		GetBlockLength() const ythrow();
		/*!
		\brief 取指定区块号的区块的长度。
		\note 无异常抛出。
		*/
		SizeType
		GetBlockLength(BlockSizeType) const ythrow();
	};

	YTextFile& File; //!< 文本文件。

private:
	const SizeType nTextSize; //!< 文本区段长度。
	const BlockSizeType nBlock; //!< 文本区块数。

public:
	/*!
	\brief 构造：使用文本文件。
	*/
	explicit
	TextFileBuffer(YTextFile&);

	/*!
	\brief 取指定区块号的区块。
	*/
	TextBlock&
	operator[](const BlockSizeType&);

	DefGetter(SizeType, TextSize, nTextSize)

	/*!
	\brief 取起始迭代器。
	\note 无异常抛出。
	*/
	HText
	begin() ythrow();

	/*!
	\brief 取终止迭代器。
	\note 无异常抛出。
	*/
	HText
	end() ythrow();
};

inline bool
operator!=(const TextFileBuffer::HText& lhs, const TextFileBuffer::HText& rhs)
	ythrow()
{
	return !(lhs == rhs);
}

inline bool
operator>(const TextFileBuffer::HText& lhs, const TextFileBuffer::HText& rhs)
	ythrow()
{
	return rhs < lhs;
}
inline bool
operator<=(const TextFileBuffer::HText& lhs, const TextFileBuffer::HText& rhs)
	ythrow()
{
	return !(rhs < lhs);
}

inline bool
operator>=(const TextFileBuffer::HText& lhs, const TextFileBuffer::HText& rhs)
	ythrow()
{
	return !(lhs < rhs);
}

inline TextFileBuffer::HText
TextFileBuffer::HText::operator-(std::ptrdiff_t o)
{
	return *this + -o;
}

inline TextFileBuffer::HText&
TextFileBuffer::HText::operator-=(std::ptrdiff_t o)
{
	return *this += -o;
}

inline SizeType
TextFileBuffer::HText::GetBlockLength() const ythrow()
{
	return GetBlockLength(blk);
}

YSL_END_NAMESPACE(Text)

YSL_END

#endif
