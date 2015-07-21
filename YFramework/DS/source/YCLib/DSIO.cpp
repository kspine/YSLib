﻿/*
	© 2015 FrankHB.

	This file is part of the YSLib project, and may only be used,
	modified, and distributed under the terms of the YSLib project
	license, LICENSE.TXT.  By continuing to use, modify, or distribute
	this file you indicate that you have read the license and
	understand and accept it fully.
*/

/*!	\file DSIO.cpp
\ingroup YCLib
\ingroup DS
\brief DS 底层输入输出接口。
\version r2424
\author FrankHB <frankhb1989@gmail.com>
\since build 604
\par 创建时间:
	2015-06-06 06:25:00 +0800
\par 修改时间:
	2015-06-30 17:33 +0800
\par 文本编码:
	UTF-8
\par 模块名称:
	YCLib_(DS)::DSIO
*/


#include "YCLib/YModules.h"
#include YFM_DS_YCLib_DSIO // for platform::Descriptions, YCL_PATH_DELIMITER,
//	YTraceDe, std::exception, ystdex::trivially_copy_n, ptrdiff_t,
//	DISC_INTERFACE, unique_raw;
#if YCL_DS
#	include <ystdex/cstdint.hpp> // for ystdex::read_uint_le,
//	ystdex::write_uint_le;
#	include <ystdex/exception.h> // for ystdex::throw_system_error,
//	std::system_error, std::runtime_error;
#	include "CHRLib/YModules.h"
#	include YFM_CHRLib_CharacterProcessing // for CHRLib::ucs2_t,
//	CHRLib::MakeUCS2LE, ystdex::ntctsnicmp, ystdex::ntctsncpy,
//	ystdex::ntctsicmp;
#	include <cerrno>
#	include <fcntl.h> // for O_RDWR, O_RDONLY, O_WRONLY, O_TRUNC, O_APPEND,
//	O_CREAT, O_EXCL;
#	include <ystdex/scope_guard.hpp> // for ystdex::guard;
#	include <ystdex/functional.hpp> // for ystdex::common_nonvoid_t,
//	ystdex::call_for_value, ystdex::bind1;
#	include <sys/iosupport.h> // for ::_reent, ::size_t, ::ssize_t, ::off_t,
//	sturct ::stat, struct ::statvfs, ::DIR_ITER, ::devoptab_t;
#endif

namespace platform_ex
{

using namespace platform;
using namespace Descriptions;

#if YCL_DS
SectorCache::SectorCache(size_t pages, size_t spps, Disc d, ::sec_t eop,
	size_t bps)
	: disc(d), number_of_pages(std::max<size_t>(pages, 2)),
	sectors_per_page_shift(std::max<size_t>(spps, 3)), end_of_partition(eop),
	bytes_per_sector(bps),
	entries(number_of_pages)
{
	entries.flush = [this](UsedListCache::value_type& pr){
		FlushEntry(pr);
	};
}
SectorCache::~SectorCache()
{
	Flush();
}

size_t
SectorCache::GetBlockCount(::sec_t sec) const ynothrow
{
	const auto end_sec(std::min<::sec_t>(
		sec + (1 << sectors_per_page_shift), end_of_partition));

	YAssert(sec <= end_sec, "Invalid range found.");
	return end_sec - sec;
}
ystdex::block_buffer*
SectorCache::GetPage(::sec_t key) ynothrow
{
	YAssert(GetKey(key) == key, "Invalid key found.");

	TryRet(&ystdex::cache_lookup(entries, key, [key, this]{
			const auto size(bytes_per_sector << sectors_per_page_shift);
			// TODO: Use aligned allocation for cache implementation.
			// NOTE: Intentionally uninitialized as libfat.
			ystdex::block_buffer entry(make_unique_default_init<byte[]>(size));
			const auto n(GetBlockCount(key));

			YAssert(n <= size, "Invalid cache creation parameter found.");
			if(YB_UNLIKELY(!disc.ReadSectors(key, n, entry.get())))
				throw std::runtime_error(
					"Failed reading on creating file system cache entry.");
			return entry;
		}))
	CatchExpr(std::exception& e, YCL_TraceRaw(Warning,
		"FATMount failure[%s]: %s", typeid(e).name(), e.what()))
	CatchExpr(..., YCL_TraceRaw(Emergent,
		"Unknown exception found when getting file system cache page."))
	return {};
}

bool
SectorCache::EraseWritePartialSector(::sec_t sec, size_t offset,
	const void* p_buf, size_t n) ynothrowv
{
	return PerformPartialSectorIO([=](ystdex::block_buffer& entry,
		size_t sec_bytes){
		ystdex::trivially_fill_n(entry.get() + sec_bytes, bytes_per_sector);
		entry.write(sec_bytes + offset, p_buf, n);
	}, sec, offset, n);
}

bool
SectorCache::FillPartialSector(::sec_t sec, size_t offset, size_t n, byte val)
	ynothrowv
{
	return PerformPartialSectorIO([=](ystdex::block_buffer& entry,
		size_t sec_bytes){
		entry.fill(sec_bytes + offset, n, val);
	}, sec, offset, n);
}

bool
SectorCache::FillSectors(::sec_t sec, size_t n, byte val) ynothrow
{
	return PerformSectorsIO([&](ystdex::block_buffer& entry, size_t sec_off,
		size_t secs_to_process){
		entry.fill(sec_off * bytes_per_sector,
			secs_to_process * bytes_per_sector, val);
	}, sec, n);
}

bool
SectorCache::Flush() ynothrow
{
	// TODO: Flush all?
	for(auto& pr : entries)
		if(!FlushEntry(pr))
			return {};
	return true;
}

bool
SectorCache::FlushEntry(UsedListCache::value_type& pr) ynothrow
{
	if(pr.second.modified())
	{
		if(!disc.WriteSectors(pr.first, GetBlockCount(pr.first),
			pr.second.get()))
			return {};
		pr.second.flush();
	}
	return true;
}

bool
SectorCache::ReadPartialSector(void* p_buf, ::sec_t sec, size_t offset,
	size_t n) ynothrowv
{
	return PerformPartialSectorIO([=](ystdex::block_buffer& entry,
		size_t sec_bytes){
		entry.read(p_buf, sec_bytes + offset, n);
	}, sec, offset, n);
}

bool
SectorCache::ReadSectors(void* p_buf, ::sec_t sec, size_t n) ynothrowv
{
	auto dst(static_cast<byte*>(p_buf));

	return PerformSectorsIO([&](ystdex::block_buffer& entry, size_t sec_off,
		size_t secs_to_process){
		entry.read(dst, sec_off * bytes_per_sector,
			secs_to_process * bytes_per_sector);
		dst += secs_to_process * bytes_per_sector;
	}, sec, n);
}

bool
SectorCache::WritePartialSector(::sec_t sec, size_t offset, const void* p_buf,
	size_t n) ynothrowv
{
	return PerformPartialSectorIO([=](ystdex::block_buffer& entry,
		size_t sec_bytes){
		entry.write(sec_bytes + offset, p_buf, n);
	}, sec, offset, n);
}

bool
SectorCache::WriteSectors(::sec_t sec, const void* p_buf, size_t n)
	ynothrow
{
	auto src(static_cast<const byte*>(p_buf));

	return PerformSectorsIO([&](ystdex::block_buffer& entry, size_t sec_off,
		size_t secs_to_process){
		entry.write(sec_off * bytes_per_sector, src,
			secs_to_process * bytes_per_sector);
		src += secs_to_process * bytes_per_sector;
	}, sec, n);
}

namespace FAT
{

//! \since build 610
//@{
using ystdex::read_uint_le;
using ystdex::write_uint_le;
using ystdex::throw_system_error;
//! \since build 611
//@{
using std::system_error;
using std::errc;
using CHRLib::ucs2_t;
// XXX: Coupled string types.
using CHRLib::MakeUCS2LE;
//@}

namespace
{

template<size_t _vWidth>
bool
ReadLEVal(SectorCache& cache, std::uint32_t& value, ::sec_t sec, size_t offset)
	ynothrow
{
	static yconstexpr const size_t
		n(_vWidth / std::numeric_limits<byte>::digits);
	byte buf[n];

	if(cache.ReadPartialSector(buf, sec, offset, n))
	{
		value = ystdex::read_uint_le<_vWidth>(buf);
		return true;
	}
	return {};
}

template<size_t _vWidth>
bool
WriteLEVal(SectorCache& cache, std::uint32_t value, ::sec_t sec, size_t offset)
	ynothrow
{
	static yconstexpr const size_t
		n(_vWidth / std::numeric_limits<byte>::digits);
	byte buf[n]{};

	write_uint_le<_vWidth>(buf, value);
	return cache.WritePartialSector(sec, offset, buf, n);
}


//! \since build 611
//@{
void
CheckThrowEIO(bool b) ythrow(system_error)
{
	if(!YB_UNLIKELY(b))
		throw_system_error(errc::io_error);
}

inline bool
MatchFATSig(const void* p)
{
	static yconstexpr const char FAT_SIG[]{'F', 'A', 'T'};

	return std::memcmp(p, FAT_SIG, sizeof(FAT_SIG)) == 0;
}

template<class _tPos>
::sec_t
PosToSec(const AllocationTable& tbl, const _tPos& pos) ynothrow
{
	return tbl.ClusterToSector(pos.GetCluster()) + pos.GetSector();
}

PDefH(void, TryFillPartialSectorOff, const AllocationTable& tbl,
	const FilePosition& pos, size_t offset, size_t n) ythrow(system_error)
	ImplExpr(CheckThrowEIO(
		tbl.Cache.FillPartialSector(PosToSec(tbl, pos), offset, n)))

PDefH(void, TryFillPartialSector, const AllocationTable& tbl,
	const FilePosition& pos, size_t n) ythrow(system_error)
	ImplExpr(TryFillPartialSectorOff(tbl, pos, pos.GetByte(), n))

PDefH(void, TryFillSectors, const AllocationTable& tbl,
	const FilePosition& pos) ythrow(system_error)
	ImplExpr(CheckThrowEIO(tbl.Cache.FillSectors(PosToSec(tbl, pos))))

template<class _tPos>
void
TryReadPartialSectorOff(const AllocationTable& tbl, const _tPos& pos,
	void* data, size_t offset, size_t n) ythrow(system_error)
{
	CheckThrowEIO(
		tbl.Cache.ReadPartialSector(data, PosToSec(tbl, pos), offset, n));
}

PDefH(void, TryReadPartialSector, const AllocationTable& tbl,
	const DEntryPosition& pos, void* data) ythrow(system_error)
	ImplExpr(TryReadPartialSectorOff(tbl, pos, data, pos.GetByte(),
		EntryDataSize))
PDefH(void, TryReadPartialSector, const AllocationTable& tbl,
	const FilePosition& pos, void* data, size_t n) ythrow(system_error)
	ImplExpr(TryReadPartialSectorOff(tbl, pos, data, pos.GetByte(), n))

PDefH(void, TryReadSectors, const AllocationTable& tbl, ClusterIndex c,
	void* buf, size_t n) ythrow(system_error)
	ImplExpr(CheckThrowEIO(
		tbl.Cache.ReadSectors(buf, tbl.ClusterToSector(c), n)))
PDefH(void, TryReadSectors, const AllocationTable& tbl,
	const FilePosition& pos, void* buf, size_t n) ythrow(system_error)
	ImplExpr(CheckThrowEIO(
		tbl.Cache.ReadSectors(buf, PosToSec(tbl, pos), n)))

template<class _tPos>
void
TryWritePartialSectorOff(const AllocationTable& tbl, const _tPos& pos,
	const void* data, size_t offset, size_t n) ythrow(system_error)
{
	CheckThrowEIO(
		tbl.Cache.WritePartialSector(PosToSec(tbl, pos), offset, data, n));
}

PDefH(void, TryWritePartialSector, const AllocationTable& tbl,
	const DEntryPosition& pos, const void* data) ythrow(system_error)
	ImplExpr(TryWritePartialSectorOff(tbl, pos, data, pos.GetByte(),
		EntryDataSize))
PDefH(void, TryWritePartialSector, const AllocationTable& tbl, const
	FilePosition& pos, const void* data, size_t n) ythrow(system_error)
	ImplExpr(TryWritePartialSectorOff(tbl, pos, data, pos.GetByte(), n))

PDefH(void, TryWriteSectors, const AllocationTable& tbl, ClusterIndex c,
	const void* buf, size_t n) ythrow(system_error) ImplRet(CheckThrowEIO(
	tbl.Cache.WriteSectors(tbl.ClusterToSector(c), buf, n)))
PDefH(void, TryWriteSectors, const AllocationTable& tbl, const FilePosition&
	pos, const void* buf, size_t n) ythrow(system_error)
	ImplRet(CheckThrowEIO(tbl.Cache.WriteSectors(PosToSec(tbl, pos), buf, n)))
//@}

} // unnamed namespace;
//@}

AllocationTable::AllocationTable(::sec_t start_sector, const byte* sec_buf,
	size_t pages, size_t sectors_per_page_shift, Disc d)
	ynothrowv
	: sectors_per_cluster(sec_buf[BPB_SecPerClus]),
	bytes_per_sector([sec_buf, this]{
		const auto t(read_uint_le<16>(sec_buf + BPB_BytsPerSec));

		// TODO: Use interval API.
		if(t < MinSectorSize || bytes_per_sector > MaxSectorSize)
			throw std::runtime_error("Unsupported sector size found.");
		return t;
	}()), bytes_per_cluster(bytes_per_sector * sectors_per_cluster),
	table_start(start_sector + read_uint_le<16>(sec_buf + BPB_RsvdSecCnt)),
	table_size([sec_buf]{
		// NOTE: last_cluster = cluster_count + Clusters::First - 1.
		const auto t(read_uint_le<16>(sec_buf + BPB_FATSz16));

		return t == 0 ? size_t(
			read_uint_le<32>(sec_buf + BPB_FATSz32)) : size_t(t);
	}()), total_sectors_num([sec_buf]{
		const auto t(read_uint_le<16>(sec_buf + BPB_TotSec16));

		return t == 0 ? read_uint_le<32>(sec_buf + BPB_TotSec32)
			: std::uint32_t(t);
	}()), limit(total_sectors_num / sectors_per_cluster),
	root_dir_start(table_start + sec_buf[BPB_NumFATs] * table_size),
	// NOTE: See Microsoft FAT Specification Section 3.5.
	root_dir_sectors_num((read_uint_le<16>(sec_buf + BPB_RootEntCnt)
	* EntryDataSize + (bytes_per_sector - 1)) / bytes_per_sector),
	data_sectors_num(total_sectors_num - size_t(root_dir_start
	+ root_dir_sectors_num - start_sector)),
	fs_type([start_sector, this]() -> FileSystemType{
		// NOTE: See Microsoft FAT Specification Section 3.5.
		ClusterIndex cluster_count(data_sectors_num / sectors_per_cluster);

		last_cluster = cluster_count + Clusters::First - 1;
		// TODO: Distinguish part type (e.g. FAT16B) with file system type. 
		if(cluster_count < Clusters::PerFAT12)
			return FileSystemType::FAT12;
		if(cluster_count < Clusters::PerFAT16)
			return FileSystemType::FAT16;
		return FileSystemType::FAT32;
	}()), root_dir_cluster([sec_buf, this]() -> ClusterIndex{
		if(fs_type != FileSystemType::FAT32)
			return Clusters::FAT16RootDirectory;

		const auto t(read_uint_le<32>(sec_buf + BPB_RootClus));
		// NOTE: Check if FAT mirroring is enabled.
		if((sec_buf[BPB_ExtFlags] & 0x80) == 0)
			// NOTE: Use the active FAT.
			table_start += table_size * (sec_buf[BPB_ExtFlags] & 0x0F);
		return t;
	}()), Cache(pages, sectors_per_page_shift, d,
	start_sector + total_sectors_num, bytes_per_sector)
{}

bool
AllocationTable::ClearLinks(ClusterIndex c) ynothrow
{
	if(IsValidCluster(c))
	{
		if(c < first_free)
			first_free = c;
		while(!Clusters::IsFreeOrEOF(c) && c != Clusters::Error)
		{
			const auto next(QueryNext(c));

			WriteEntry(c, Clusters::Free);
			if(free_cluster < limit)
				++free_cluster;
			c = next;
		}
		return true;
	}
	return {};
}

ClusterIndex
AllocationTable::CountFreeCluster() const ynothrow
{
	size_t count(0);

	for(ClusterIndex cur_cluster(Clusters::First); cur_cluster <= last_cluster;
		++cur_cluster)
		if(QueryNext(cur_cluster) == Clusters::Free)
			++count;
	return count;
}

ClusterIndex
AllocationTable::LinkFree(ClusterIndex c) ynothrow
{
	if(last_cluster < c)
		return Clusters::Error;

	const auto cur_link(QueryNext(c));

	if(IsValidCluster(cur_link))
		return cur_link;

	auto first_valid(std::max<ClusterIndex>(first_free, Clusters::First));
	bool loop_around = {};

	while(QueryNext(first_valid) != Clusters::Free)
		if(last_cluster < ++first_valid)
		{
			if(loop_around)
			{
				first_free = first_valid;
				return Clusters::Error;
			}
			else
			{
				first_valid = Clusters::First;
				loop_around = true;
			}
		}
	first_free = first_valid;
	if(free_cluster != 0)
		--free_cluster;
	last_alloc_cluster = first_valid;
	if(IsValidCluster(c))
		WriteEntry(c, first_valid);
	WriteEntry(first_valid, Clusters::EndOfFile);
	return first_valid;
}

ClusterIndex
AllocationTable::ProbeForNext(ClusterIndex c) const ynothrow
{
	// FIXME: Too large malformed parameter?
	YAssert(c != Clusters::Free, "Invalid parameter found.");

	ClusterIndex next_cluster(Clusters::Free);

	switch(fs_type)
	{
	case FileSystemType::FAT12:
		{
			const auto c32(c * 3 / 2);
			::sec_t sector(table_start + c32 / bytes_per_sector);
			size_t offset(c32 % bytes_per_sector);

			ReadLEVal<8>(Cache, next_cluster, sector, offset);
			if(++offset == bytes_per_sector)
			{
				offset = 0;
				++sector;
			}

			std::uint32_t next_cluster_h(0);

			ReadLEVal<8>(Cache, next_cluster_h, sector, offset);
			next_cluster |= next_cluster_h << 8;
		}
		if(c & 0x01)
			next_cluster = next_cluster >> 4;
		else
			next_cluster &= 0x0FFF;
		if(Clusters::MaxValid12 < next_cluster)
			next_cluster = Clusters::EndOfFile;
		break;
	case FileSystemType::FAT16:
		ReadLEVal<16>(Cache, next_cluster, table_start
			+ (c << 1) / bytes_per_sector, c % (bytes_per_sector >> 1) << 1);
		if(Clusters::MaxValid16 < next_cluster)
			next_cluster = Clusters::EndOfFile;
		break;
	case FileSystemType::FAT32:
		ReadLEVal<32>(Cache, next_cluster, table_start
			+ (c << 2) / bytes_per_sector, c % (bytes_per_sector >> 2) << 2);
		if(Clusters::MaxValid32 < next_cluster)
			next_cluster = Clusters::EndOfFile;
		break;
	default:
		return Clusters::Error;
	}
	return next_cluster;
}

ClusterIndex
AllocationTable::QueryNext(ClusterIndex c) const ynothrow
{
	// FIXME: c == Clusters::EndOfFile?
	return c == Clusters::Free ? ClusterIndex(Clusters::Free) : ProbeForNext(c);
}

ClusterIndex
AllocationTable::QueryLast(ClusterIndex c) const ynothrow
{
	// TODO: Assert !Clusters::IsFreeOrEOF(c)?
	ClusterIndex t;

	do 
	{
		t = c;
		c = QueryNext(t);
	}while(!Clusters::IsFreeOrEOF(c));
	return t;
}

void
AllocationTable::ReadClusters(const byte* sec_buf) ynothrowv
{
	yunseq(
	free_cluster = read_uint_le<32>(sec_buf + FSI_Free_Count),
	last_alloc_cluster = read_uint_le<32>(sec_buf + FSI_Nxt_Free)
	);
}

ClusterIndex
AllocationTable::TrimChain(ClusterIndex start, size_t len)
	ynothrow
{
	if(len == 0)
	{
		ClearLinks(start);
		return Clusters::Free;
	}
	--len;

	auto next(QueryNext(start));

	while(len > 0 && next != Clusters::Free && next != Clusters::EndOfFile)
	{
		--len;
		start = next;
		next = QueryNext(start);
	}
	if(next != Clusters::Free && next != Clusters::EndOfFile)
		ClearLinks(next);
	WriteEntry(start, Clusters::EndOfFile);
	return start;
}

ClusterIndex
AllocationTable::TryLinkFree(ClusterIndex c) ythrow(system_error)
{
	const auto t(LinkFree(c));

	if(IsValidCluster(t))
		return t;
	throw_system_error(errc::no_space_on_device);
}

void
AllocationTable::WriteClusters(byte* sec_buf) const ynothrowv
{
	write_uint_le<32>(sec_buf + FSI_Free_Count, free_cluster),
	write_uint_le<32>(sec_buf + FSI_Nxt_Free, last_alloc_cluster);
}

bool
AllocationTable::WriteEntry(ClusterIndex c, std::uint32_t val) const
	ynothrow
{
	if(IsValidCluster(c))
	{
		switch(fs_type)
		{
		case FileSystemType::FAT12:
			{
				const auto c32(c * 3 / 2);
				::sec_t sector(table_start + (c32 / bytes_per_sector));
				size_t offset(c32 % bytes_per_sector);
				std::uint32_t old_val;

				if((c & 0x01) != 0)
				{
					ReadLEVal<8>(Cache, old_val, sector, offset);
					val = val << 4 | (old_val & 0x0F);
					WriteLEVal<8>(Cache, byte(val), sector, offset);
					if(++offset >= bytes_per_sector)
						yunseq(offset = 0, ++sector);
					WriteLEVal<8>(Cache, byte(val >> 8), sector, offset);
				}
				else
				{
					WriteLEVal<8>(Cache, val, sector, offset);
					if(++offset >= bytes_per_sector)
						yunseq(offset = 0, ++sector);
					ReadLEVal<8>(Cache, old_val, sector, offset);
					val = ((val >> 8) & 0x0F) | (old_val & 0xF0);
					WriteLEVal<8>(Cache, val, sector, offset);
				}
			}
			break;
		case FileSystemType::FAT16:
			WriteLEVal<16>(Cache, val, table_start + (c << 1)
				/ bytes_per_sector, (c % (bytes_per_sector >> 1)) << 1);
			break;
		case FileSystemType::FAT32:
			WriteLEVal<32>(Cache, val, table_start + (c << 2)
				/ bytes_per_sector, (c % (bytes_per_sector >> 2)) << 2);
			break;
		default:
			return {};
		}
		return true;
	}
	return {};
}

void
AllocationTable::WriteFSInfo(byte* sec_buf) ynothrowv
{
	YAssertNonnull(sec_buf);
	// FIXME: Big endian implementations.
	write_uint_le<32>(sec_buf + FSI_LeadSig, FSI_LeadSig_Value),
	write_uint_le<32>(sec_buf + FSI_StrucSig, FSI_StrucSig_Value);
	free_cluster = CountFreeCluster();
	WriteClusters(sec_buf);
	write_uint_le<32>(sec_buf + FSI_TrailSig, FSI_TrailSig_Value);
}


void
FilePosition::Decompose(std::uint32_t pos, size_t bytes_per_cluster,
	size_t bytes_per_sector) ynothrowv
{
	YAssert(bytes_per_cluster != 0, "Invalid parameter found."),
	YAssert(bytes_per_sector != 0, "Invalid parameter found.");
	yunseq(sector = pos % bytes_per_cluster / bytes_per_sector,
		byte = pos % bytes_per_sector);
}


DEntry::DEntry(Partition& part)
	: NamePos{DEntryPosition{0}, DEntryPosition{0}}
{
	// TODO: Optimize.
	name = ".";
	Data.SetupRoot(part.GetRootDirCluster());
}
DEntry::DEntry(Partition& part, const NamePosition& name_pos)
	: NamePos(name_pos)
{
	auto pos(name_pos[0]);
	bool finished{};
	ucs2_t long_name[LFN::MaxLength];
	EntryData edata;

	while(!finished)
	{
		TryReadPartialSector(part.Table, pos, edata.data());
		if(pos == name_pos[1])
		{
			Data = edata;
			finished = true;
		}
		else
			edata.CopyLFN(long_name);
		if(!part.IncrementPosition(pos))
			throw_system_error(errc::io_error);
	}
	name = name_pos[0] == name_pos[1] ? Data.GenerateAlias()
		: LFN::ConvertToMBCS(long_name);
}
DEntry::DEntry(Partition& part, const char* path)
	: DEntry(part, path, std::strchr(Nonnull(path), char()))
{}
DEntry::DEntry(Partition& part, const char* path, const char* path_end)
{
	YAssertNonnull(path_end);

	ClusterIndex dclus;
	const char* path_pos = Nonnull(path);
	bool found = {};

	if(path_pos[0] == YCL_PATH_DELIMITER)
	{
		dclus = part.GetRootDirCluster();
		while(path_pos[0] == YCL_PATH_DELIMITER)
			++path_pos;
		// NOTE: Return directory "/".
		if(path_pos == path_end)
			goto found_root;
	}
	else
		dclus = part.GetCWDCluster();
	do
	{
		const char* next_pos(std::strchr(path_pos, YCL_PATH_DELIMITER));
		size_t dname_len(next_pos ? next_pos - path_pos
			: std::strlen(path_pos));

		if(LFN::MaxMBCSLength < dname_len)
			throw_system_error(errc::filename_too_long);

		// NOTE: Fake entries for "." or ".." when the dclus is root
		//	cluster.
		if(dclus == part.GetRootDirCluster()
			&& (std::strncmp(".", path_pos, dname_len) == 0
			|| std::strncmp("..", path_pos, dname_len) == 0))
		{
			this->~DEntry();
			::new(this) DEntry(part);
		}
		else
		{
			NamePos = GenerateBeforeFirstNamePos(dclus);
			do
			{
				if(!QueryNextFrom(part))
					throw_system_error(errc::no_such_file_or_directory);
				try
				{
					if(dname_len == name.length() && ystdex::ntctsnicmp(
						MakeUCS2LE(string(path_pos, dname_len)).c_str(),
						MakeUCS2LE(name).c_str(), dname_len) == 0)
						found = true;
				}
				CatchIgnore(...)
				if(!found)
					try
					{
						if(Data.FindAlias(path_pos, dname_len))
							found = true;
					}
					CatchIgnore(...)
					{}
				if(found && !Data.IsDirectory() && next_pos)
					found = {};
			}while(!found);
		}
		if(!next_pos || (next_pos >= path_end))
			found = true;
		else if(Data.IsDirectory())
		{
			dclus = part.EntryGetCluster(Data);
			if(dclus == Clusters::Root)
				dclus = part.GetRootDirCluster();
			path_pos = next_pos;
			while(path_pos[0] == YCL_PATH_DELIMITER)
				++path_pos;
			found = path_pos >= path_end;
		}
	}while(!found);
	// NOTE: On FAT32 an actual cluster should be specified for the root
	//	entry, not cluster 0 as on FAT16.
	if(part.GetFileSystemType() == FileSystemType::FAT32 && Data.IsDirectory()
		&& part.EntryGetCluster(Data) == Clusters::Root)
	{
found_root:
		this->~DEntry();
		::new(this) DEntry(part);
	}
}
DEntry::DEntry(int, Partition& part, const char* path,
	ClusterIndex& parent_clus)
{
	const char* path_end(std::strrchr(Nonnull(path), YCL_PATH_DELIMITER));

	if(!path_end)
	{
		parent_clus = part.GetCWDCluster();
		path_end = path;
	}
	else 
	{
		this->~DEntry();
		::new(this) DEntry(part, path, path_end);
		if(Data.IsDirectory())
		{
			parent_clus = part.EntryGetCluster(Data);
			++path_end;
		}
		else
			throw_system_error(errc::not_a_directory);
	}
	name = ystdex::rtrim(string(path_end), ' ');
#	if 0
	ystdex::ltrim(name, ' ');
#	endif
	if(YB_UNLIKELY(LFN::MaxMBCSLength < name.length()))
		throw_system_error(errc::filename_too_long);
	if(name.empty()
		|| name.find_first_of(LFN::IllegalCharacters) != string::npos)
		throw_system_error(errc::invalid_argument);
}

void
DEntry::AddTo(Partition& part, ClusterIndex dclus)
{
	const auto name_len(name.length());

	YAssert(name_len == ystdex::ntctslen(name.c_str())
		&& !(LFN::MaxMBCSLength < name_len) && name_len != 0
		&& name.find_first_of(LFN::IllegalCharacters)
		== string::npos, "Invalid name found.");
	// NOTE: Make sure the name does not contain any control codes or
	//	codes not representable in UCS-2.
	for(const char c : name)
		if(c < 0x20 || static_cast<unsigned char>(c) >= 0xF0)
			throw_system_error(errc::invalid_argument);
	if(!part.EntryExists(name, dclus))
	{
		Data.ClearAlias();

		EntryDataUnit alias_check_sum(0);
		size_t entry_size;

		if(name == ".")
		{
			Data.SetDot(0),
			entry_size = 1;
		}
		if(name == "..")
		{
			Data.SetDot(0),
			Data.SetDot(1),
			entry_size = 1;
		}
		else
		{
			const auto& long_name(MakeUCS2LE(name));
			const auto len(long_name.length());

			if(len < LFN::MaxLength)
			{
				auto alias_tp(LFN::ConvertToAlias(long_name));
				string pri(get<0>(alias_tp)), alias(pri);
				const auto& ext(get<1>(alias_tp));

				if(!ext.empty())
					alias += '.' + ext;

				const auto alias_len(get<2>(alias_tp) ? alias.length() : 0);

				if(alias_len == 0)
					entry_size = 1;
				else
				{
					entry_size = (len + LFN::EntryLength - 1) / LFN::EntryLength
						+ 1;
					if(ystdex::ntctsnicmp(alias.c_str(), name.c_str(),
						LFN::MaxAliasLength) != 0
						|| part.EntryExists(alias, dclus))
					{
						pri.resize(LFN::MaxAliasMainPartLength, '_');
						alias = pri + '.' + ext;

						size_t i(1);

						do
						{
							if(YB_UNLIKELY(LFN::MaxNumericTail < i))
								throw_system_error(errc::invalid_argument);
							LFN::WriteNumericTail(alias, i++);
						}while(part.EntryExists(alias, dclus));
					}
				}
				Data.WriteAlias(alias);
			}
			else
				throw_system_error(errc::invalid_argument);
			alias_check_sum = LFN::GenerateAliasChecksum(Data.data());
		}
		FindEntryGap(part, dclus, entry_size);

		auto cur_entry_pos(NamePos[0]);
		EntryData long_name_entry;
		const auto& long_name(MakeUCS2LE(name));
		bool valid(true);

		for(size_t i(entry_size); valid && i > 0;
			yunseq(valid = part.IncrementPosition(cur_entry_pos), --i))
		{
			if(i > 1)
			{
				long_name_entry[LFN::Ordinal] = (i - 1)
					| (i == entry_size ? LFN::LastLongEntry : 0);
				for(size_t j(0); j < LFN::EntryLength; ++j)
					// NOTE: Padding vs terminating null character.
					write_uint_le<16>(long_name_entry.data()
						+ LFN::OffsetTable[j], long_name[(i - 2)
						* LFN::EntryLength + j] == ucs2_t() ? (j > 1
						&& long_name[(i - 2) * LFN::EntryLength + j - 1]
						== ucs2_t() ? 0xFFFF : 0x0000)
						: long_name[(i - 2) * LFN::EntryLength + j]);
				yunseq(
				long_name_entry[LFN::CheckSum] = alias_check_sum,
				long_name_entry[LFN::Flag]
					= EntryDataUnit(Attribute::LongFileName),
				long_name_entry[LFN::Reserved1] = 0
				);
				write_uint_le<16>(long_name_entry.data() + LFN::Reserved2, 0);
				TryWritePartialSector(part.Table, cur_entry_pos,
					long_name_entry.data());
			}
			else
				TryWritePartialSector(part.Table, cur_entry_pos, Data.data());
		}
	}
	else
		throw_system_error(errc::file_exists);
}

void
DEntry::FindEntryGap(Partition& part, ClusterIndex dclus, size_t size)
	ythrow(system_error)
{
	// NOTE: Scan for free entry.
	DEntryPosition gap_end(dclus), gap_start(gap_end);
	bool end_of_dir = {};
	auto dentry_remain(size);
	EntryData edata;

	if(size != 0)
		while(true)
		{
			TryReadPartialSector(part.Table, gap_end, edata.data());
			if(edata[0] == EntryData::Last || edata[0] == EntryData::Free)
			{
				if(dentry_remain == size)
					gap_start = gap_end;
				--dentry_remain;
				end_of_dir = edata[0] == EntryData::Last;
			}
			else
				dentry_remain = size;
			if(!end_of_dir && dentry_remain > 0)
				part.ExtendPosition(gap_end);
			else
				break;
		}
	NamePos[0] = gap_start;
	if(end_of_dir)
	{
		edata.FillLast();
		// NOTE: Increase by one to take account of End Of Directory Marker.
		++dentry_remain;
		while(dentry_remain > 0)
		{
			NamePos[1] = gap_end;
			part.ExtendPosition(gap_end);
			// NOTE: Fill the entry with blanks.
			TryWritePartialSector(part.Table, gap_end, edata.data());
			--dentry_remain;
		}
	}
	else
		NamePos[1] = gap_end;
}

bool
DEntry::QueryNextFrom(Partition& part) ythrow(system_error)
{
	auto estart(NamePos[1]);

	if(estart.IsFAT16RootCluster())
		estart.SetCluster(part.GetRootDirCluster());

	auto eend(estart);
	EntryData edata;
	ucs2_t long_name[LFN::MaxLength];
	bool has_long_name = {};
	EntryDataUnit chk_sum = 0;

	while(part.IncrementPosition(eend))
	{
		TryReadPartialSector(part.Table, eend, edata.data());
		if(edata.IsLongFileName())
		{
			const auto ord(edata[LFN::Ordinal]);

			// NOTE: See Microsoft FAT Specification Section 7.1.
			if((ord & 0x80) != 0)
				// NOTE: Corrupted. 
				has_long_name = {};
			else if((ord & LFN::LastLongEntry) != 0)
				yunseq(
				estart = eend, has_long_name = true,
				long_name[std::min<size_t>((ord & ~LFN::LastLongEntry)
					* LFN::EntryLength, LFN::MaxLength - 1)] = char(),
				chk_sum = edata[LFN::CheckSum]
				);
			else if(chk_sum != edata[LFN::CheckSum])
				has_long_name = {};
			if(has_long_name)
				edata.CopyLFN(long_name);
		}
		else if(edata.IsVolume())
			;
		else if(edata[0] == EntryData::Last)
			break;
		else if(edata[0] != EntryData::Free && edata[0] > 0x20
			&& !edata.IsVolume())
		{
			if(has_long_name
				&& LFN::GenerateAliasChecksum(edata.data()) != chk_sum)
			{
				has_long_name = {};
				name.clear();
			}
			if(has_long_name)
				TryExpr(name = LFN::ConvertToMBCS(long_name))
				CatchRet(..., {})
			else
			{
				estart = eend;
				TryExpr(name = edata.GenerateAlias())
				CatchExpr(..., name.clear())
			}
			yunseq(NamePos[0] = estart, NamePos[1] = eend, Data = edata);
			return true;
		}
	}
	return {};
}


Partition::Partition(Disc d, size_t pages, size_t sectors_per_page_shift,
	::sec_t start_sector)
	// TODO: Use aligned allocation.
	// NOTE: Uninitialized intentionally here to make it behave as libfat.
	: Partition(make_unique_default_init<byte[]>(MaxSectorSize).get(),
	d, pages, sectors_per_page_shift, start_sector)
{}
Partition::Partition(byte* sec_buf, Disc d, size_t pages,
	size_t sectors_per_page_shift, ::sec_t start_sector)
	: disc(d), Table([&, sec_buf]{
		if(!ReadSector(start_sector, sec_buf))
			throw std::runtime_error("Failed reading sectors.");
		if(sec_buf[Signature_word] != 0x55
			|| sec_buf[Signature_word + 1] != 0xAA)
			throw std::runtime_error("Invalid MBR or boot sector found.");
		if(start_sector == 0 && !MatchFATSig(sec_buf + FAT16::BS_FilSysType)
			&& !MatchFATSig(sec_buf + FAT32::BS_FilSysType))
		{
			start_sector = FindFirstValidPartition(sec_buf);
			if(!ReadSector(start_sector, sec_buf))
				throw std::runtime_error(
					"Failed reading sectors for finding start sector.");
		}
		if(!MatchFATSig(sec_buf + FAT16::BS_FilSysType)
			&& !MatchFATSig(sec_buf + FAT32::BS_FilSysType))
			throw std::runtime_error("Failed verifying FAT part.");
		GetMutexRef().lock();
		ystdex::ntctsncpy(label.data(), sec_buf + (MatchFATSig(sec_buf
			+ FAT16::BS_FilSysType) ? size_t(FAT16::BS_VolLab)
			: size_t(FAT32::BS_VolLab)), label.size());
		return start_sector;
	}(), sec_buf, pages, sectors_per_page_shift, d),
	fs_info_sector(start_sector + (read_uint_le<16>(sec_buf + BPB_FSInfo)
	? read_uint_le<16>(sec_buf + BPB_FSInfo) : 1)),
	read_only((disc.GetFeatures() & FEATURE_MEDIUM_CANWRITE) == 0),
	cwd_cluster(GetRootDirCluster())
{
	if(GetFileSystemType() == FileSystemType::FAT32)
		ReadFSInfo();
	GetMutexRef().unlock();
}
Partition::~Partition()
{
	try
	{
		lock_guard<mutex> lck(GetMutexRef());
		for(const auto& file : open_files)
		{
			// XXX: Flush after exceptions occurred?
			const int e(file.get().SyncToDisc());

			if(YB_UNLIKELY(e != 0))
				YTraceDe(Err,
					"Synchronization of file failed @ Partition::~Partition.");
		}
		if(GetFileSystemType() == FileSystemType::FAT32)
			WriteFSInfo();
	}
	CatchExpr(std::exception& e, YCL_TraceRaw(Warning,
		"Partition::~Partition failure[%s]: %s", typeid(e).name(), e.what()))
	CatchExpr(..., YCL_TraceRaw(Emergent,
		"Unknown exception found @ Partition::~Partition."))
}

void
Partition::CheckPositionForNextCluster(FilePosition& pos) ythrow(system_error)
{
	const auto s(pos.GetSector());
	const auto spc(GetSectorsPerCluster());

	if(YB_UNLIKELY(spc < s))
		// NOTE: Internal error.
		throw_system_error(errc::invalid_argument);
	if(s == spc)
	{
		auto t(Table.QueryNext(pos.GetCluster()));

		pos = {Clusters::IsFreeOrEOF(t) ? Table.TryLinkFree(pos.GetCluster()) : t,
			0, pos.GetByte()};
	}
}

void
Partition::ChangeDir(const char* path) ythrow(system_error)
{
	DEntry entry(*this, path);

	if(entry.Data.IsDirectory())
		cwd_cluster = EntryGetCluster(entry.Data);
	else
		throw_system_error(errc::not_a_directory);
}

void
Partition::CreateFSInfo()
{
	YAssert(GetFileSystemType() != FileSystemType::FAT32,
		"Only FAT32 supported.");
	if(!read_only)
	{
		// TODO: Use aligned allocation.
		auto p_sector_buf(make_unique<byte[]>(GetBytesPerSector()));
		const auto sec_buf(p_sector_buf.get());

		Table.WriteFSInfo(sec_buf);
		if(YB_UNLIKELY(!disc.WriteSectors(fs_info_sector, 1, sec_buf)))
			YTraceDe(Err,
				"Writing sectors failed on creating file system information.");
	}
}

bool
Partition::EntryExists(const string& name, ClusterIndex dclus)
	ythrow(system_error)
{
	const auto dir_name_len(name.length());

	if(dir_name_len < LFN::MaxMBCSLength)
	{
		DEntry entry(dclus);
		bool found_file(entry.QueryNextFrom(*this));

		while(found_file)
		{
			try
			{
				if(dir_name_len == entry.GetName().length()
					&& ystdex::ntctsicmp(MakeUCS2LE(name).c_str(),
					MakeUCS2LE(entry.GetName()).c_str()) == 0)
					return true;
			}
			CatchIgnore(...)
			try
			{
				if(entry.Data.FindAlias(name.c_str(), LFN::MaxAliasLength))
					return true;
			}
			CatchIgnore(...)
			found_file = entry.QueryNextFrom(*this);
		}
	}
	return {};
}

ClusterIndex
Partition::EntryGetCluster(const EntryData& data) const ynothrow
{
	// NOTE: Only use high 16 bits of start cluster when we are certain they are
	//	correctly defined.
	const auto dst(data.data());
	const auto t(read_uint_le<16>(dst + EntryData::Cluster));

	return GetFileSystemType() == FileSystemType::FAT32
		? t | (read_uint_le<16>( dst + EntryData::ClusterHigh) << 16) : t;
}

::sec_t
Partition::FindFirstValidPartition(byte* sec_buf) const ynothrowv
{
	if(ReadSector(0, sec_buf))
	{
		byte part_table[16 * 4];
		byte* ptr(part_table);

		// TODO: Resolve magic numbers.
		ystdex::trivially_copy_n(sec_buf + 0x1BE, 16 * 4, part_table);
		for(size_t i(0); i < 4; yunseq(++i, ptr += 16))
		{
			::sec_t part_lba = read_uint_le<32>(ptr + 0x8);

			if(MatchFATSig(sec_buf + FAT16::BS_FilSysType)
				|| MatchFATSig(sec_buf + FAT32::BS_FilSysType))
				return part_lba;
			if(ptr[4] == 0)
				continue;
			if(ptr[4] == 0x0F)
			{
				::sec_t part_lba2(part_lba), next_lba2(0);

				// NOTE: Up to 8 logic partitions.
				for(size_t n = 0; n < 8; ++n)
				{
					if(!ReadSector(part_lba + next_lba2, sec_buf))
						return 0;
					// TODO: Resolve magic numbers.
					part_lba2 = part_lba + next_lba2
						+ read_uint_le<32>(sec_buf + 0x1C6);
					next_lba2 = read_uint_le<32>(sec_buf + 0x1D6);
					if(!ReadSector(part_lba2, sec_buf))
						return 0;
					if(MatchFATSig(sec_buf + FAT16::BS_FilSysType)
						|| MatchFATSig(sec_buf + FAT32::BS_FilSysType))
						return part_lba2;
					if(next_lba2 == 0)
						break;
				}
			}
			else
			{
				if(ReadSector(part_lba, sec_buf))
					return 0;
				if(MatchFATSig(sec_buf + FAT16::BS_FilSysType)
					|| MatchFATSig(sec_buf + FAT32::BS_FilSysType))
					return part_lba;
			}
		}
	}
	return 0;
}

bool
Partition::IncrementPosition(DEntryPosition& entry_pos) ynothrow
{
	auto pos(entry_pos);

	if(pos.IncOffset() == GetBytesPerSector() / EntryDataSize)
	{
		pos.IncSectorAndResetOffset();

		const auto s(pos.GetSector());
		const bool root(pos.IsFAT16RootCluster());

		if(!root && s == GetSectorsPerCluster())
		{
			pos.SetSector(0);

			const auto c(Table.QueryNext(pos.GetCluster()));

			if(c == Clusters::EndOfFile)
				return {};
			pos.SetCluster(c);
		}
		else if(root && s == Table.GetRootDirSectorsNum())
			return {};
	}
	entry_pos = pos;
	return true;
}

void
Partition::ExtendPosition(DEntryPosition& entry_pos) ythrow(system_error)
{
	auto pos(entry_pos);

	if(pos.IncOffset() == GetBytesPerSector() / EntryDataSize)
	{
		pos.IncSectorAndResetOffset();

		const auto s(pos.GetSector());
		const bool root(pos.IsFAT16RootCluster());

		if(!root && s == GetSectorsPerCluster())
		{
			pos.SetSector(0);

			ClusterIndex c(Table.QueryNext(pos.GetCluster()));

			if(c == Clusters::EndOfFile)
				c = LinkFreeClusterCleared(pos.GetCluster());
			pos.SetCluster(c);
		}
		else if(root && s == Table.GetRootDirSectorsNum())
			throw_system_error(errc::no_space_on_device);
	}
	entry_pos = pos;
}

void
Partition::MakeDir(const char* path) ythrow(system_error)
{
	try
	{
		DEntry dentry(*this, path);

		throw_system_error(errc::file_exists);
	}
	catch(system_error& e)
	{
		if(e.code().value() == ENOENT)
		{
			if(read_only)
				throw_system_error(errc::read_only_file_system);

			ClusterIndex parent_clus;
			DEntry dentry(0, *this, path, parent_clus);

			dentry.Data.Clear();
			dentry.Data.SetDirectoryAttribute();
			dentry.Data.WriteDateTime();
	
			auto dclus(LinkFreeClusterCleared(Clusters::Free));

			dentry.Data.WriteCluster(dclus);
			// XXX: Only %ENOSPCE may be expected.
			// NOTE: Long name and short name would be set by
			//	%DEntry::FindEntryGap.
			dentry.AddTo(*this, parent_clus);

			// NOTE: Create the dot entry within the directory.
			EntryData edata;
			auto& cache(GetCacheRef());
			const auto dir_sector(Table.ClusterToSector(dclus));

			edata.SetupRoot(dclus);
			// XXX: POSIX does not require %EIO here.
			if(!YB_UNLIKELY(cache.EraseWritePartialSector(dir_sector, 0,
				edata.data(), EntryDataSize)))
				throw_system_error(errc::no_space_on_device);
			// NOTE: Create the double dot entry within the directory.
			edata.SetDot(EntryData::Name + 1);
			// NOTE: if parent directory is root then ".." always link to
			//	Cluster 0.
			edata.WriteCluster(parent_clus == GetRootDirCluster()
				? ClusterIndex(Clusters::FAT16RootDirectory) : parent_clus);
			if(!YB_UNLIKELY(cache.WritePartialSector(dir_sector, EntryDataSize,
				edata.data(), EntryDataSize)))
				throw_system_error(errc::no_space_on_device);
			Flush();
		}
		else
			throw;
	}
}

void
Partition::ReadFSInfo()
{
	YAssert(GetFileSystemType() == FileSystemType::FAT32,
		"Only FAT32 supported.");

	// TODO: Use aligned allocation.
	auto p_sector_buf(make_unique<byte[]>(GetBytesPerSector()));
	const auto sec_buf(p_sector_buf.get());

	if(ReadFSInfoSector(sec_buf))
	{
		if(read_uint_le<32>(sec_buf + FSI_LeadSig) != FSI_LeadSig_Value
			|| read_uint_le<32>(sec_buf + FSI_StrucSig) != FSI_StrucSig_Value
			|| read_uint_le<32>(sec_buf + FSI_Free_Count) == 0)
			CreateFSInfo();
		else
			Table.ReadClusters(sec_buf);
	}
}

void
Partition::Rename(const char* old, const char* new_name) ythrow(system_error)
{
	if(FetchPartitionFromPath(new_name) == this)
	{
		// FIXME: errc::invalid_argument: The old pathname names an
		//	ancestor directory of the new pathname. 
		if(!read_only)
		{
			DEntry old_dir_entry(*this, old);
			try
			{
				DEntry new_dir_entry(*this, new_name);

				throw_system_error(errc::file_exists);
			}
			catch(system_error& e)
			{
				if(e.code().value() == ENOENT)
				{
					ClusterIndex parent_clus;
					DEntry dentry(0, *this, new_name, parent_clus);

					yunseq(dentry.Data = old_dir_entry.Data,
						dentry.NamePos = old_dir_entry.NamePos);
					// XXX: Only %ENOSPC may be expected?
					// NOTE: Long name and short name would be set by
					//	%DEntry::FindEntryGap.
					dentry.AddTo(*this, parent_clus);
					RemoveEntry(old_dir_entry.NamePos);
					Flush();
				}
				else
					throw;
			}
		}
		else
			throw_system_error(errc::read_only_file_system);
	}
	else
		throw_system_error(errc::cross_device_link);
}

void
Partition::StatFS(struct ::statvfs& st)
{
	// NOTE: Special command was given to sync the free_cluster.
	if(std::memcmp(&st.f_flag, "SCAN", 4) == 0
		&& GetFileSystemType() == FileSystemType::FAT32)
		CreateFSInfo();

	size_t free_count(Table.GetFreeClusters());

	yunseq(
	// NOTE: FAT clusters = POSIX blocks.
	st.f_bsize = GetBytesPerCluster(),
	st.f_frsize = GetBytesPerCluster(),
	st.f_blocks = Table.GetUsedClusters(),
	st.f_bfree = free_count,
	st.f_bavail = free_count,
	// NOTE: Treat requests for info on inodes as clusters.
	st.f_files = Table.GetUsedClusters(),
	st.f_ffree = free_count,
	st.f_favail = free_count,
	st.f_fsid = disc.GetType(),
	// NOTE: No support for %ST_ISUID and %ST_ISGID.
	st.f_flag = ST_NOSUID | (read_only ? ST_RDONLY : 0),
	st.f_namemax = LFN::MaxMBCSLength
	);
}

void
Partition::RemoveEntry(const DEntry::NamePosition& np) ythrow(system_error)
{
	EntryData edata;
	bool finished;
	auto name_pos(np);

	// NOTE: Create an empty directory entry to overwrite the old ones.
	do
	{
		TryReadPartialSector(Table, name_pos[0], edata.data());
		edata[0] = EntryData::Free;
		TryWritePartialSector(Table, name_pos[0], edata.data());
		finished = name_pos[0] == name_pos[1];
		if(!IncrementPosition(name_pos[0]))
			throw_system_error(errc::io_error);
	}while(!finished);
}

void
Partition::StatFromEntry(const EntryData& data, struct ::stat& st) const
	ynothrow
{
	const auto dst(data.data());

	yunseq(
		st.st_dev = disc.GetType(),
		// NOTE: The file serial number is the start cluster.
		st.st_ino = ::ino_t(EntryGetCluster(data)),
		st.st_mode = (data.IsDirectory() ? S_IFDIR : S_IFREG)
			| (S_IRUSR | S_IRGRP | S_IROTH) | (data.IsWritable()
			? (S_IWUSR | S_IWGRP | S_IWOTH) : 0),
		// NOTE: Always one hard link on a FAT file.
		st.st_nlink = 1,
		// NOTE: Faked for FAT.
		st.st_uid = 1, st.st_gid = 2,
		st.st_size = read_uint_le<32>(dst + EntryData::FileSize),
		st.st_atime
			= ConvertFileTime(read_uint_le<16>(dst + EntryData::ADate), 0),
		st.st_spare1 = 0, st.st_mtime = ConvertFileTime(read_uint_le<16>(
			dst + EntryData::MDate), read_uint_le<16>(dst + EntryData::MTime)),
		st.st_spare2 = 0, st.st_ctime = ConvertFileTime(read_uint_le<16>(
			dst + EntryData::CDate), read_uint_le<16>(dst + EntryData::CTime)),
		st.st_spare3 = 0, st.st_blksize = GetBytesPerSector(),
		st.st_spare4[0] = 0, st.st_spare4[1] = 0
	);

	const auto bps(GetBytesPerSector());

	yunseq(st.st_rdev = st.st_dev, st.st_blocks = (st.st_size + bps - 1) / bps);
}

bool
Partition::Sync(const FileInfo& file) const ythrow(system_error)
{
	EntryData dir_entry_data;
	const auto data(dir_entry_data.data());
	const auto& dir_entry_end(file.GetNamePos()[1]);

	TryReadPartialSector(Table, dir_entry_end, data);

	const auto c(file.GetStartCluster());

	write_uint_le<32>(data + EntryData::FileSize, file.GetSize()),
	dir_entry_data.WriteCluster(c);

	const auto date_time(FetchDateTime());

	write_uint_le<16>(data + EntryData::MTime, date_time.second),
	write_uint_le<16>(data + EntryData::MDate, date_time.first),
	write_uint_le<16>(data + EntryData::ADate, date_time.first);
	data[EntryData::Attributes] |= EntryDataUnit(Attribute::Archive);

	TryWritePartialSector(Table, dir_entry_end, data);

	return GetCacheRef().Flush();
}

ClusterIndex
Partition::LinkFreeClusterCleared(ClusterIndex c) ythrow(system_error)
{
	const auto t(Table.LinkFree(c));

	if(t != Clusters::Free && t != Clusters::Error)
	{
		auto& cache(GetCacheRef());

		for(size_t i(0); i < GetSectorsPerCluster(); ++i)
			if(YB_UNLIKELY(!cache.FillSectors(Table.ClusterToSector(t) + i)))
				throw_system_error(errc::io_error);
		if(Table.IsValidCluster(t))
			return t;
	}
	throw_system_error(errc::no_space_on_device);
}

void
Partition::Stat(struct ::stat& st, const char* path) ythrow(system_error)
{
	StatFromEntry(DEntry(*this, path).Data, st);
}

void
Partition::Unlink(const char* path) ythrow(system_error)
{
	if(!read_only)
	{
		DEntry dentry(*this, path);
		const auto c(EntryGetCluster(dentry.Data));

		if(dentry.Data.IsDirectory())
		{
			DEntry dir_contents(c);
			bool next_entry(dir_contents.QueryNextFrom(*this));

			while(next_entry)
				if(dir_contents.IsDot())
					next_entry = dir_contents.QueryNextFrom(*this);
				else
					throw_system_error(errc::operation_not_permitted);
		}
		if(Table.IsValidCluster(c) && !Table.ClearLinks(c))
		{
			RemoveEntry(dentry.NamePos);
			Flush();
			return;
		}
		throw_system_error(errc::io_error);
	}
	throw_system_error(errc::read_only_file_system);
}

void
Partition::WriteFSInfo() const
{
	YAssert(GetFileSystemType() != FileSystemType::FAT32,
		"Only FAT32 supported.");

	// TODO: Use aligned allocation.
	auto p_sector_buf(make_unique<byte[]>(GetBytesPerSector()));
	const auto sec_buf(p_sector_buf.get());

	if(!ReadFSInfoSector(sec_buf))
		throw std::runtime_error(
			"Failed reading first sector on writing file system information.");
	// FIXME: Big endian implementations.
	if(read_uint_le<32>(sec_buf + FSI_LeadSig) != FSI_LeadSig_Value
		|| read_uint_le<32>(sec_buf + FSI_StrucSig) != FSI_StrucSig_Value)
		throw std::runtime_error("Failed validating FSInfo signature on"
			" writing file system information.");
	Table.WriteClusters(sec_buf);
	if(!WriteSector(fs_info_sector, sec_buf))
		throw std::runtime_error(
			"Failed writing file system information sector.");
}


Partition*
FetchPartitionFromPath(const char* path) ynothrow
{
	if(const auto p_devops = ::GetDeviceOpTab(Nonnull(path)))
		return static_cast<Partition*>(p_devops->deviceData);
	return {};
}


DirState::DirState(const char* path) ythrow(system_error)
	: DirState([=]() -> Partition&{
		if(const auto p = FetchPartitionFromPath(path))
			return *p;
		throw_system_error(errc::no_such_device);
	}(), path, unique_lock<mutex>(GetPartitionRef().GetMutexRef()))
{}
DirState::DirState(Partition& part, const char* path, unique_lock<mutex>)
	// XXX: Extended partition mutex to lock directory states.
	ythrow(system_error)
	: part_ref(part), start_cluster([&]{
		path = CheckColons(path);

		DEntry dentry(part, path);

		if(dentry.Data.IsDirectory())
			return part.EntryGetCluster(dentry.Data);
		throw_system_error(errc::not_a_directory);
	}()), current_entry(start_cluster),
	valid_entry(current_entry.QueryNextFrom(GetPartitionRef()))
{}

void
DirState::Iterate(char* filename, struct ::stat* filestat)
	ythrow(system_error)
{
	if(valid_entry)
	{
		auto& part(GetPartitionRef());

		// TODO: Optimize.
		ystdex::ntctsncpy(Nonnull(filename), current_entry.GetName().c_str(),
			std::min<size_t>(current_entry.GetName().length() + 1, NAME_MAX));
		if(filestat)
			part.StatFromEntry(current_entry.Data, *filestat);
		valid_entry = current_entry.QueryNextFrom(part);
	}
	else
		throw_system_error(errc::no_such_file_or_directory);
}

void
DirState::Reset() ythrow(system_error)
{
	current_entry.NamePos = DEntry::GenerateBeforeFirstNamePos(start_cluster);
	valid_entry = current_entry.QueryNextFrom(GetPartitionRef());
}


FileInfo::FileInfo(Partition& part, const char* path, int flags)
{
	switch(flags & 0x03)
	{
	case O_RDWR:
		attr.set(WriteBit);
	case O_RDONLY:
		attr.set(ReadBit);
		break;
	case O_WRONLY:
		attr.set(WriteBit);
		break;
	default:
		throw_system_error(errc::permission_denied);
	}
	// NOTE: %O_CREAT when file not exist would be check later.
	// FIXME: POSIX requires %EROFS for %O_TRUNC on a read-only file system.
	if(CanWrite() && part.IsReadOnly())
		throw_system_error(errc::read_only_file_system);

	lock_guard<mutex> lck(part.GetMutexRef());
	const auto do_init([&, this](DEntry& dentry){
		file_size = dentry.Data.ReadFileSize();
#if 0
		// NOTE: Allow LARGEFILEs with undefined results. Make sure that the
		//	file size can fit in the available space.
		if((flags & O_LARGEFILE) == 0 && file_size >= (1 << 31))
			ThrowSystemError(EFBIG);
#endif
		// XXX: Extension on file level.
		if(CanWrite() && !dentry.Data.IsWritable())
			throw_system_error(errc::read_only_file_system);
		yunseq(part_ptr = &part,
			start_cluster = part.EntryGetCluster(dentry.Data));
		if((flags & O_TRUNC) != 0 && CanWrite() && start_cluster != 0)
		{
			part.Table.ClearLinks(start_cluster);
			yunseq(start_cluster = Clusters::Free, file_size = 0,
				attr.set(ModifiedBit));
		}
		yunseq(name_pos = dentry.NamePos, current_position = 0,
			rw_position = {start_cluster});
		if(flags & O_APPEND)
		{
			attr.set(AppendBit);
			// Set append pointer to the end of the file
			// TODO: Use %FilePosition::Decompose.
			append_position = {part.Table.QueryLast(start_cluster), file_size
				% part.GetBytesPerCluster() / part.GetBytesPerSector(),
				file_size % part.GetBytesPerSector()};
			// NOTE: Check if the end of the file is on the end of a cluster.
			if(file_size > 0 && file_size % part.GetBytesPerCluster() == 0)
				// NOTE: Set flag to allocate a new cluster
				append_position = {append_position.GetCluster(),
					part.GetSectorsPerCluster(), 0};
		}
		else
			// NOTE: Guarantee all members initialized.
			append_position = rw_position;
		Deref(part.LockOpenFiles()).insert(*this);
	});
	try
	{
		DEntry dentry(part, path);
		
		if((flags & O_CREAT) != 0 && (flags & O_EXCL) != 0)
			throw_system_error(errc::file_exists);
		// FIXME: POSIX requires %EISDIR only when %O_WRONLY or %O_RDWR
		//	included in the flag.
		if(dentry.Data.IsDirectory())
			throw_system_error(errc::is_a_directory);
		do_init(dentry);
	}
	catch(system_error& e)
	{
		if(e.code().value() == ENOENT)
		{
			if((flags & O_CREAT) != 0)
			{
				if(!part.IsReadOnly())
				{
					ClusterIndex parent_clus;
					DEntry dentry(0, part, path, parent_clus);

					dentry.Data.Clear();
					dentry.Data.WriteCDateTime();
					// FIXME: Only %ENOSPC may be expected.
					// NOTE: Long name and short name would be set by
					//	%DEntry::FindEntryGap.
					dentry.AddTo(part, parent_clus);
					attr.set(ModifiedBit);
					do_init(dentry);
				}
				else
					throw_system_error(errc::read_only_file_system);
			}
			else
				throw_system_error(errc::no_such_file_or_directory);
		}
		else
			throw;
	}
}

void
FileInfo::CheckAppend() ynothrow
{
	if(CanAppend())
		append_position = rw_position;
}

void
FileInfo::Extend() ythrow(system_error)
{
	auto& part(GetPartitionRef());
	// NOTE: It is assumed that %start_cluster is always valid. This will be
	//	true when %Extend is called from %write_r.
	// TODO: Use %FilePosition::Decompose?
	const auto bpc(part.GetBytesPerCluster());
	FilePosition file_pos{part.Table.QueryLast(start_cluster),
		file_size % bpc / part.GetBytesPerSector(), file_size % bpc};

	YAssert(file_size < current_position, "Invalid position found.");

	size_t remain(current_position - file_size);

	if(remain > 0 && file_size > 0 && file_pos.GetSector() == 0
		&& file_pos.GetByte() == 0)
		file_pos = {part.Table.TryLinkFree(file_pos.GetCluster()), 0,
			file_pos.GetByte()};
	if(remain + file_pos.GetByte() < part.GetBytesPerSector())
	{
		// NOTE: Only need to clear to the end of the sector.
		TryFillPartialSector(part.Table, file_pos, remain);
		file_pos.AddByte(remain);
	}
	else
	{
		if(file_pos.GetByte() > 0)
		{
			const auto delta(part.GetBytesPerSector() - file_pos.GetByte());

			TryFillPartialSector(part.Table, file_pos, delta);
			remain -= delta;
			file_pos.IncSectorAndResetOffset();
		}
		while(remain >= part.GetBytesPerSector())
		{
			if(file_pos.GetSector() >= part.GetSectorsPerCluster())
			file_pos = {part.Table.TryLinkFree(file_pos.GetCluster()),
				0, file_pos.GetByte()};
			TryFillSectors(part.Table, file_pos);
			remain -= part.GetBytesPerSector();
			file_pos.IncSector();
		}
		if(remain != 0)
		{
			part.CheckPositionForNextCluster(file_pos);
			file_pos.SetByte(remain);
			TryFillPartialSectorOff(part.Table, file_pos, 0, remain);
		}
	}
	rw_position = file_pos;
	file_size = current_position;
}

void
FileInfo::Shrink() ynothrow
{
	yunseq(start_cluster = Clusters::Free, append_position = {Clusters::Free});
}

int
FileInfo::SyncToDisc() ynothrow
{
	if(CanWrite() && IsModified() && !GetPartitionRef().Sync(*this))
		return EIO;
	attr.set(ModifiedBit, {});
	return 0;
}

void
FileInfo::Stat(struct ::stat& st) const
{
	auto& part(GetPartitionRef());
	EntryData edata;

	TryReadPartialSector(part.Table, name_pos[1], edata.data());
	part.StatFromEntry(edata, st);
	// NOTE: Update fields that have changed since the file was opened.
	yunseq(st.st_ino = ::ino_t(start_cluster), st.st_size = file_size);
}

void
FileInfo::Truncate(FileSize new_size) ythrow(system_error)
{
	auto& part(GetPartitionRef());

	if(new_size > GetSize())
	{
		if(start_cluster == Clusters::Free)
		{
			ClusterIndex t(part.Table.TryLinkFree(Clusters::Free));

			yunseq(start_cluster = t, rw_position = {t});
		}

		const FilePosition saved_position(GetRWPosition());
		const FileSize saved_offset(GetCurrentPosition());

		SetCurrentPosition(new_size);
		Extend();
		CheckAppend();
		SetRWPosition(saved_position),
		SetCurrentPosition(saved_offset);
	}
	else if(new_size < GetSize())
	{
		if(new_size == 0)
		{
			part.Table.ClearLinks(GetStartCluster());
			Shrink();
		}
		else
		{
			// NOTE: Trim the file down to the required size. Drop the unneeded
			//	end of the cluster chain. If the end falls on a cluster
			//	boundary, drop that cluster too, then set a flag to allocate
			//	a cluster as needed.
			const auto last_cluster(part.Table.TrimChain(GetStartCluster(),
				(new_size - 1) / part.GetBytesPerCluster() + 1));

			if(CanAppend())
			{
				const auto re(new_size % part.GetBytesPerCluster());

				SetAppendPosition({last_cluster, re == 0
					? part.GetSectorsPerCluster()
					: re / part.GetBytesPerSector(),
					new_size % part.GetBytesPerSector()});
			}
		}
	}
	UpdateSize(new_size);
}

::ssize_t
FileInfo::TryRead(char* buf, size_t nbyte) ythrow(system_error)
{
	auto& part(GetPartitionRef());

	if(nbyte != 0 && current_position < file_size
		&& start_cluster != Clusters::Free)
	{
		if(file_size < nbyte + current_position)
			nbyte = file_size - current_position;

		auto remain(nbyte);
		auto pos(rw_position);
		// NOTE: Align to sector.
		auto tmp(std::min<size_t>(part.GetBytesPerSector() - pos.GetByte(),
			remain));

		if(tmp < part.GetBytesPerSector())
		{
			TryReadPartialSector(part.Table, pos, buf, tmp);
			yunseq(remain -= tmp, buf += tmp),
			pos.AddByte(tmp);
			if(pos.GetByte() >= part.GetBytesPerSector())
				pos.IncSectorAndResetOffset();
		}
		// NPTE: Align to cluster, %tmp is number of sectors to read.
		tmp = remain > (part.GetSectorsPerCluster() - pos.GetSector())
			* part.GetBytesPerSector() ? part.GetSectorsPerCluster()
			- pos.GetSector() : remain / part.GetBytesPerSector();
		if(tmp > 0)
		{
			TryReadSectors(part.Table, pos, buf, tmp);

			const auto n(tmp * part.GetBytesPerSector());

			yunseq(buf += n, remain -= n),
			pos.AddSector(tmp);
		}
		// NOTE: Move onto next cluster. It should get to here without reading
		//	anything if a cluster is due to be allocated.
		if(pos.GetSector() >= part.GetSectorsPerCluster())
		{
			const auto t(part.Table.QueryNext(pos.GetCluster()));

			if(remain == 0 && t == Clusters::EndOfFile)
				pos.SetSector(part.GetSectorsPerCluster());
			else if(!part.Table.IsValidCluster(t))
				throw_system_error(errc::io_error);
			else
				pos = {t, 0, pos.GetByte()};
		}
		// NOTE: Read in whole clusters, contiguous blocks at a time.
		while(remain >= part.GetBytesPerCluster())
		{
			ClusterIndex chunk_end, next_chunk_start(pos.GetCluster());
			size_t chunkSize(0);

			do
			{
				chunk_end = next_chunk_start;
				next_chunk_start = part.Table.QueryNext(chunk_end);
				chunkSize += part.GetBytesPerCluster();
			}while(next_chunk_start == chunk_end + 1
				&& chunkSize + part.GetBytesPerCluster() <= remain);
			TryReadSectors(part.Table, pos.GetCluster(), buf,
				chunkSize / part.GetBytesPerSector());
			buf += chunkSize;
			remain -= chunkSize;
			// Advance to next cluster
			if(remain == 0 && next_chunk_start == Clusters::EndOfFile)
				pos = {chunk_end, part.GetSectorsPerCluster(), pos.GetByte()};
			else if(!part.Table.IsValidCluster(next_chunk_start))
				throw_system_error(errc::io_error);
			else
				pos = {next_chunk_start, 0, pos.GetByte()};
		}
		// NOTE: %tmp is number of sectors left.
		tmp = remain / part.GetBytesPerSector();
		if(tmp > 0)
		{
			TryReadSectors(part.Table, pos.GetCluster(), buf, tmp);

			const auto n(tmp * part.GetBytesPerSector());

			yunseq(buf += n, remain -= n),
			pos.AddSector(tmp);
		}
		if(remain > 0)
		{
			TryReadPartialSectorOff(part.Table, pos, buf, 0, remain);
			pos.AddByte(remain);
			remain = 0;
		}
		nbyte -= remain;
		yunseq(rw_position = pos, current_position += nbyte);
		return nbyte;		
	}
	return 0;
}

::off_t
FileInfo::TrySeek(::off_t offset, int whence) ythrow(system_error)
{
	::off_t new_pos(offset);

	switch(whence)
	{
	case SEEK_SET:
		break;
	case SEEK_CUR:
		new_pos += ::off_t(current_position);
		break;
	case SEEK_END:
		new_pos += ::off_t(file_size);
		break;
	default:
		throw_system_error(errc::invalid_argument);
	}

	auto& part(GetPartitionRef());

	if(!(0 < offset && new_pos < 0))
	{
		// NOTE: %new_pos can only be larger than the FILE_MAX_SIZE on platforms
		//	where ::off_t is larger than 32 bits.
		if(!(new_pos < 0
			|| (sizeof(new_pos) > 4 && new_pos > ::off_t(MaxFileSize))))
		{
			auto pos(static_cast<std::uint32_t>(new_pos));

			if(pos <= file_size && start_cluster != Clusters::Free)
			{
				ptrdiff_t clus_count(pos / part.GetBytesPerCluster());
				ClusterIndex cluster(start_cluster);

				if(pos >= current_position)
				{
					ptrdiff_t
						cur_count(current_position / part.GetBytesPerCluster());
					if(rw_position.GetSector() == part.GetSectorsPerCluster())
						--cur_count;
					yunseq(clus_count -= cur_count,
						cluster = rw_position.GetCluster());
				}
				rw_position.Decompose(pos, part.GetBytesPerCluster(),
					part.GetBytesPerSector());
				for(auto next_cluster(part.Table.QueryNext(cluster));
					clus_count > 0 && !Clusters::IsFreeOrEOF(next_cluster);
					next_cluster = part.Table.QueryNext(cluster))
				{
					--clus_count;
					cluster = next_cluster;
				}
				if(clus_count > 0)
				{
					if(clus_count == 1 && file_size == pos
						&& rw_position.GetSector() == 0)
						rw_position = {rw_position.GetCluster(),
							part.GetSectorsPerCluster(), 0};
					else
						throw_system_error(errc::invalid_argument);
				}
				rw_position.SetCluster(cluster);
			}
			current_position = pos;
			return pos;
		}
		throw_system_error(errc::invalid_argument);
	}
	throw_system_error(errc::value_too_large);
}

::ssize_t
FileInfo::TryWrite(const char* buf, size_t nbyte) ythrow(system_error)
{
	// FIXME: Only write up to the maximum file size, taking into
	//	account wrap-around of integers. Overflow?
	if(nbyte + file_size > MaxFileSize || nbyte + file_size < file_size)
		nbyte = MaxFileSize - file_size;
	if(nbyte != 0)
	{
		auto& part(GetPartitionRef());
		size_t remain(nbyte);

		if(start_cluster == Clusters::Free)
		{
			start_cluster = part.Table.TryLinkFree(Clusters::Free);
			yunseq(append_position = {start_cluster},
				rw_position = {start_cluster});
		}

		bool flag_appending(CanAppend());
		auto pos(flag_appending ? append_position : rw_position);

		if(!flag_appending)
		{
			if(current_position > file_size)
				Extend();
			if(nbyte + current_position > file_size)
				flag_appending = true;
		}
		if(remain != 0)
			part.CheckPositionForNextCluster(pos);

		// NOTE: Align to sector.
		size_t tmp(std::min<size_t>(part.GetBytesPerSector() - pos.GetByte(),
			remain));

		if(tmp < part.GetBytesPerSector())
		{
			TryWritePartialSector(part.Table, pos, buf, tmp);
			yunseq(remain -= tmp, buf += tmp);
			pos.AddByte(tmp);
			if(pos.GetByte() >= part.GetBytesPerSector())
				pos.IncSectorAndResetOffset();
		}
		// NOTE: Align to sector. %tmp is number of sectors to write.
		tmp = remain > (part.GetSectorsPerCluster() - pos.GetSector())
			* part.GetBytesPerSector() ? part.GetSectorsPerCluster()
			- pos.GetSector() : remain / part.GetBytesPerSector();
		if(tmp > 0 && tmp < part.GetSectorsPerCluster())
		{
			TryWriteSectors(part.Table, pos, buf, tmp);

			const auto n(tmp * part.GetBytesPerSector());

			yunseq(buf += n, remain -= n),
			pos.AddSector(tmp);
		}
		while(remain >= part.GetBytesPerCluster())
		{
			if(remain != 0)
				part.CheckPositionForNextCluster(pos);

			auto chunk_end(pos.GetCluster()), next_chunk_start(chunk_end);
			size_t chunk_size(part.GetBytesPerCluster());
			FilePosition next_position(pos);

			// NOTE: Group consecutive clusters.
			while(chunk_size + part.GetBytesPerCluster() < remain)
			{
				// NOTE: Pretend to use up all sectors in %next_position.
				next_position.SetSector(part.GetSectorsPerCluster());
				// TODO: != ?
				if(remain > chunk_size)
					part.CheckPositionForNextCluster(next_position);
				next_chunk_start = next_position.GetCluster();
				if(next_chunk_start != chunk_end + 1)
					break;
				yunseq(chunk_end = next_chunk_start,
					chunk_size += part.GetBytesPerCluster());
			}
			TryWriteSectors(part.Table, pos.GetCluster(), buf,
				chunk_size / part.GetBytesPerSector());
			yunseq(buf += chunk_size, remain -= chunk_size);
			if(chunk_end != next_chunk_start
				&& part.Table.IsValidCluster(next_chunk_start))
				// NOTE: New cluster is already allocated (because it was not
				//	consecutive).
				pos = {next_chunk_start, 0, pos.GetByte()};
			else
				pos = {chunk_end, part.GetSectorsPerCluster(), pos.GetByte()};
		}
		if(remain != 0)
			part.CheckPositionForNextCluster(pos);
		// NOTE: %tmp is number of sectors left.
		tmp = remain / part.GetBytesPerSector();
		if(tmp > 0)
		{
			TryWriteSectors(part.Table, pos.GetCluster(), buf, tmp);

			const auto n(tmp * part.GetBytesPerSector());

			yunseq(buf += n, remain -= n),
			pos.AddSector(tmp);
		}
		if(remain > 0)
		{
			if(flag_appending)
				part.GetCacheRef().EraseWritePartialSector(
					PosToSec(part.Table, pos), 0, buf, remain);
			else
				TryWritePartialSectorOff(part.Table, pos, buf, 0, remain);
			pos.AddByte(remain);
			remain = 0;
		}
		yunseq(nbyte -= remain, attr.set(ModifiedBit));
		if(CanAppend())
		{
			append_position = pos;
			file_size += nbyte;
		}
		else
		{
			rw_position = pos;
			current_position += nbyte;
			if(file_size < current_position)
				file_size = current_position;
		}
	}
	return nbyte;
}

//! \since build 611
namespace
{

template<typename _func, typename... _tParams>
using FilterRes = ystdex::common_nonvoid_t<ystdex::result_of_t<
	_func(_tParams&&...)>, int>;

inline PDefH(int, seterr, int& re, int e) ynothrow
	ImplRet(re = e, -1)
inline YB_NONNULL(1) PDefH(int, seterr, ::_reent* r, int e) ynothrow
	// NOTE: The newlib reentrant environment pointer shall not be null in a
	//	%::devoptab_t operation.
	ImplRet(Deref(r)._errno = e, -1)

template<typename _func>
auto
FilterDevOps(::_reent* r, _func f) ynothrowv -> FilterRes<_func>
{
	using fresult = decltype(f());
	using result = ystdex::common_nonvoid_t<fresult, int>;
	static yconstexpr const auto
		def_val(std::is_pointer<fresult>::value ? result() : result(-1));

	TryRet(ystdex::call_for_value(0, f))
	CatchExpr(std::system_error& e, seterr(r, e.code().value()))
	CatchExpr(std::bad_alloc&, seterr(r, ENOMEM))
	CatchExpr(..., YAssert(false, "Invalid exception found"))
	return def_val;
}

template<typename _func>
YB_NONNULL(1) auto
op_path(::_reent* r, const char*& path, _func f) ynothrowv
	-> FilterRes<_func, Partition&>
{
	return FilterDevOps(r, [=, &path]{
		if(Deref(path) != char())
		{
			if(auto p_part = FetchPartitionFromPath(path))
			{
				// XXX: %EINVAL is not POSIX error for path.
				path = CheckColons(path);
				return f(*p_part);
			}
			// XXX: This is not POSIX error.
			throw_system_error(std::errc::no_such_device);
		}
		throw_system_error(ENOENT);
	});
}

template<typename _func>
YB_NONNULL(1) auto
op_path_locked(::_reent* r, const char*& path, _func f) ynothrowv
	-> FilterRes<_func, Partition&>
{
	return op_path(r, path, [f](Partition& part){
		lock_guard<mutex> lck(part.GetMutexRef());

		f(part);
		return 0;
	});
}

template<typename _func>
YB_NONNULL(1) auto
op_dir_locked(::_reent* r, ::DIR_ITER* dir_state, _func f) ynothrowv
	-> FilterRes<_func, DirState&>
{
	return FilterDevOps(r, [=]{
		auto& state(
			Deref(static_cast<DirState*>(Deref(dir_state).dirStruct)));
		// XXX: Extended partition mutex to lock directory states.
		lock_guard<mutex> lck(state.GetPartitionRef().GetMutexRef());

		return f(state);
	});
}

bool
check_true(const FileInfo&) ynothrow
{
	return true;
}

template<typename _func, typename _fCheck = bool(*)(const FileInfo&)>
YB_NONNULL(1) auto
op_file_locked(::_reent* r, int fd, _func f, _fCheck check = check_true)
	ynothrowv -> FilterRes<_func, FileInfo&>
{
	return FilterDevOps(r, [=]{
		// NOTE: Check of %fd is similar to %::close_r.
		auto& file(Deref(reinterpret_cast<FileInfo*>(fd)));

		if(check(file))
		{
			lock_guard<mutex> lck(file.GetMutexRef());

			return f(file);
		}
		throw_system_error(EBADF);
	});
}

// NOTE: %REENTRANT_SYSCALLS_PROVIDED is configured for libgloss for
//	arm-*-*-eabi targets in devkitPro ports. See source
//	'newlib/libgloss/configure'.

using std::mem_fn;
using std::ref;
using ystdex::bind1;

const ::devoptab_t dotab_fat{
	"fat", sizeof(FileInfo), [](::_reent* r, void* file_struct,
		const char* path, int flags, int) YB_NONNULL(1, 2) ynothrowv{
		// NOTE: Before call of %::devoptab_t::open_r, the parameter
		//	%file_struct is filled as a pointer after handle in implementation
		//	of devkitPro port of newlib (libsysbase) %::_open_r, which is
		//	assumed to be not null.
		return op_path(r, path, [=, &path](Partition& part){
			::new(Nonnull(file_struct)) FileInfo(part, path, flags);

			return int(file_struct);
		});
	}, [](::_reent* r, int fd) YB_NONNULL(1) ynothrowv{
		// NOTE: The parameter %filedes is actually cast from the file structure
		//	pointer stored by %devoptab_t::open_r and this function is called
		//	when the reference count in handle decreased to zero. Since this
		//	pointer would not be null for non-predefined streams unless
		//	corrupted, check for null (yield %EBADF) is not performed, as same
		//	as libfat did. Also the name is %fd, not POSIX %filedes.
		return FilterDevOps(r, [=]{
			auto& file_info(Deref(reinterpret_cast<FileInfo*>(fd)));
			auto& part(file_info.GetPartitionRef());
			const auto guard(ystdex::make_guard([&]{
				Deref(part.LockOpenFiles()).erase(file_info);
				file_info.~FileInfo();
			}));
			int e(0);
			{
				lock_guard<mutex> lck(part.GetMutexRef());

				if(file_info.CanWrite())
					e = file_info.SyncToDisc();
			}
			return e == 0 ? 0 : seterr(r, e);
		});
	}, [](::_reent* r, int fd, const char* buf, size_t nbyte) YB_NONNULL(2, 4)
		ynothrowv{
		return op_file_locked(r, fd, bind1(&FileInfo::TryWrite, buf, nbyte),
			mem_fn(&FileInfo::CanWrite));
	}, [](::_reent* r, int fd, char* buf, size_t nbyte) YB_NONNULL(2, 4)
		ynothrowv{
		return op_file_locked(r, fd, bind1(&FileInfo::TryRead, buf, nbyte),
			mem_fn(&FileInfo::CanRead));
	}, [](::_reent* r, int fd, ::off_t offset, int whence) YB_NONNULL(1)
		ynothrowv{
		return
			op_file_locked(r, fd, bind1(&FileInfo::TrySeek, offset, whence));
	}, [](::_reent* r, int fd, struct ::stat* buf) YB_NONNULL(2, 4)
		ynothrowv{
		return op_file_locked(r, fd, bind1(&FileInfo::Stat, ref(Deref(buf))));
	}, [](::_reent* r, const char* path, struct ::stat* buf) YB_NONNULL(1, 2, 3)
		ynothrowv{
		return op_path_locked(r, path,
			bind1(&Partition::Stat, ref(Deref(buf)), ref(path)));
	}, [](::_reent* r, const char*, const char*) YB_NONNULL(1, 2, 3) ynothrowv{
		return seterr(r, ENOTSUP);
	}, [](::_reent* r, const char* path) YB_NONNULL(1, 2) ynothrowv{
		return op_path_locked(r, path, bind1(&Partition::Unlink, ref(path)));
	}, [](::_reent* r, const char* path) YB_NONNULL(1, 2) ynothrowv{
		return op_path_locked(r, path, bind1(&Partition::ChangeDir, ref(path)));
	}, [](::_reent* r, const char* old, const char* new_name)
		YB_NONNULL(1, 2, 3) ynothrowv{
		return op_path_locked(r, old,
			bind1(&Partition::Rename, ref(old), ref(new_name)));
	}, [](::_reent* r, const char* path, int) YB_NONNULL(1, 2) ynothrowv{
		return op_path_locked(r, path, bind1(&Partition::MakeDir, ref(path)));
	}, sizeof(DirState), [](::_reent* r, ::DIR_ITER* dir_state,
		const char* path) YB_NONNULL(1, 2, 3) ynothrowv{
		return FilterDevOps(r, [=]() -> ::DIR_ITER*{
			const auto p(dir_state->dirStruct);

			::new(Nonnull(p)) DirState(path);
			return static_cast<::DIR_ITER*>(p);
		});
	}, [](::_reent* r, ::DIR_ITER* dir_state) YB_NONNULL(1, 2) ynothrowv{
		return op_dir_locked(r, dir_state, mem_fn(&DirState::Reset));
	}, [](::_reent* r, ::DIR_ITER* dir_state, char* filename,
		struct ::stat* filestat) YB_NONNULL(1, 2, 3) ynothrowv{
		// NOTE: The filename is of %NAME_MAX characters in newlib DS port.
		return op_dir_locked(r, dir_state,
			bind1(&DirState::Iterate, filename, filestat));
	}, [](::_reent* r, ::DIR_ITER* dir_state) YB_NONNULL(1, 2) ynothrowv{
		return op_dir_locked(r, dir_state,
			[=](DirState& state) ynothrow{
				state.~DirState();
			});
	}, [](::_reent* r, const char* path, struct ::statvfs* buf)
		YB_NONNULL(1, 2, 3) ynothrowv{
		return
			op_path_locked(r, path, bind1(&Partition::StatFS, ref(Deref(buf))));
	}, [](::_reent* r, int fd, ::off_t length) YB_NONNULL(1) ynothrowv -> int{
		return length >= 0 ? (sizeof(length) <= 4 || length <= ::off_t(
			MaxFileSize) ? op_file_locked(r, fd, bind1(&FileInfo::Truncate,
			std::uint32_t(length)), mem_fn(&FileInfo::CanWrite)) : EFBIG)
			: EINVAL;
	}, [](::_reent* r, int fd) YB_NONNULL(1) ynothrowv -> int{
		return op_file_locked(r, fd, mem_fn(&FileInfo::SyncToDisc));
	}, nullptr, nullptr, nullptr, nullptr
};

} // unnamed namespace;

bool
Mount(const string& name, const ::DISC_INTERFACE& intf, ::sec_t start_sector,
	size_t pages, size_t sectors_per_page_shift)
{
	if(intf.startup() && intf.isInserted())
	{
		const auto devname(name + ':');

		if(::FindDevice(devname.c_str()) >= 0)
			return true;
		if(auto p = unique_raw(static_cast<::devoptab_t*>(::operator new(
			sizeof(::devoptab_t) + name.length() + 1)),
			static_cast<void(&)(void*)>(::operator delete)))
		{
			if(auto p_part = make_unique<Partition>(intf, pages,
				sectors_per_page_shift, start_sector))
			{
				const auto p_name(reinterpret_cast<char*>(p.get() + 1));

				yunseq(ystdex::trivially_copy_n(&dotab_fat, 1, p.get()),
					ystdex::trivially_copy_n(name.c_str(), name.length() + 1,
					p_name), p->name = p_name, p->deviceData = p_part.get());
				if(YB_UNLIKELY(::AddDevice(p.get())) == -1)
					throw std::runtime_error("Adding device failed.");
				p_part.release();
				p.release();
				return true;
			}
		}
		YTraceDe(Alert, "Memory allocation failure @ FATMount.");
	}
	return {};
}

bool
Unmount(const char* name) ynothrow
{
	if(const auto p_devops = ::GetDeviceOpTab(Nonnull(name)))
		if(p_devops->open_r == dotab_fat.open_r && ::RemoveDevice(name) != -1)
		{
			::delete static_cast<Partition*>(p_devops->deviceData);
			::operator delete(const_cast<::devoptab_t*>(p_devops));
			return true;
		}
	return {};
}

}
#endif

} // namespace platform_ex;
