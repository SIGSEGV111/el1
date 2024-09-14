#include "def.hpp"
#ifdef EL_OS_CLASS_POSIX

#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include "io_file.hpp"
#include <sys/mman.h>

namespace el1::io::file
{
	const TUTF32 TPath::SEPERATOR('/');
	const char* const TPath::PARENT_DIR = "..";
	const char* const TPath::CURRENT_DIR = ".";

	int TAccess::FileOpenFlags() const
	{
		if(read && write)
			return O_RDWR;
		else if(read)
			return O_RDONLY;
		else if(write)
			return O_WRONLY;
		else
			return 0;
	}

	static bool IsSameInode(const THandle& h1, const THandle& h2)
	{
		struct stat st1 = {};
		struct stat st2 = {};

		EL_SYSERR(fstat(h1, &st1));
		EL_SYSERR(fstat(h2, &st2));

		return st1.st_ino == st2.st_ino && st1.st_dev == st2.st_dev;
	}

	void TPath::Validate()
	{
		usys_t n_backref = 0;

		for(usys_t i = 0; i < components.Count(); i++)
		{
			EL_ERROR(components[i].Find(TPath::SEPERATOR) != NEG1, TInvalidPathException, *this, i, TInvalidPathException::EReason::COMPONENT_CONTAINS_SEPERATOR);

			if(components[i] == TPath::PARENT_DIR)
				n_backref++;
		}

		EL_ERROR(IsAbsolute() && n_backref > components.Count() / 2, TInvalidPathException, *this, 0, TInvalidPathException::EReason::POINTS_BEFORE_ROOT);

		for(usys_t i = 1; i < components.Count(); i++)
			EL_ERROR(components[i].Length() == 0, TInvalidPathException, *this, i, TInvalidPathException::EReason::EMPTY_COMPONENT);
	}

	void TPath::Simplify()
	{
		// ./test/../blub
		// => blub

		/*
		 * TPath test = {"some", "path", "..", "..", "file"};
		 * const TPath expect = {"file"};
		 */

		this->Validate();
		this->components.RemoveItem(TPath::CURRENT_DIR, NEG1);

		for(usys_t i = 1; i < this->components.Count(); i++)
			if(i >= 1 && this->components[i] == TPath::PARENT_DIR && this->components[i-1] != TPath::PARENT_DIR)
			{
				EL_ERROR(i == 1 && IsAbsolute(), TInvalidPathException, *this, 1, TInvalidPathException::EReason::POINTS_BEFORE_ROOT);
				this->components.Remove(i - 1, 2);
				i -= 2;
			}

		this->cached_cstr.reset();
	}

	bool TPath::IsAbsolute() const
	{
		if(this->components.Count() == 0)
			return false;
		else
			return this->components[0].Length() == 0;
	}

	TPath TPath::CurrentWorkingDirectory()
	{
		char buffer[4096 + 1];
		char* const pwd = getcwd(buffer, sizeof(buffer));
		EL_ERROR(pwd == nullptr, TSyscallException, errno);
		return TPath(pwd);
	}

	void TPath::MakeAbsolute()
	{
		if(this->IsRelative())
		{
			components.Insert(0, CurrentWorkingDirectory().components);
		}
	}

	bool TPath::IsMountpoint() const
	{
		if(IsAbsolute() && this->components.Count() == 1)
			return true;	// root

		struct stat st1 = {};
		struct stat st2 = {};

		EL_SYSERR(stat(*this, &st1));
		EL_SYSERR(stat(this->Parent(), &st2));

		return st1.st_dev != st2.st_dev;
	}

	void TPath::CreateAsDirectory(const bool recursive) const
	{
		if(recursive)
		{
			char* cstr = (char*)(const char*)*this;
			TList<usys_t> pos;

			for(usys_t i = 0; cstr[i] != 0; i++)
				if(cstr[i] == '/')
				{
					pos.Append(i);
					cstr[i] = 0;
				}

			EL_ERROR(mkdir(cstr, 0777) != 0 && errno != EEXIST, TSyscallException, errno);
			for(usys_t i = 0; i < pos.Count(); i++)
			{
				cstr[pos[i]] = '/';
				EL_ERROR(mkdir(cstr, 0777) != 0 && errno != EEXIST, TSyscallException, errno);
			}

			struct stat st = {};
			EL_SYSERR(stat(cstr, &st));
			EL_ERROR(!S_ISDIR(st.st_mode), TSyscallException, EEXIST);
		}
		else
		{
			EL_SYSERR(mkdir(*this, 0777));
		}
	}

	/************************************************************************/

	bool TDirectory::IsRoot() const
	{
		return this->Parent() == *this;
	}

	bool TDirectory::IsMountpoint() const
	{
		return this->Parent().FileSystem().id != this->FileSystem().id;
	}

	u64_t TDirectory::ObjectID() const
	{
		struct stat st = {};
		EL_SYSERR(fstat(this->handle, &st));
		return st.st_ino;
	}

	bool TDirectory::operator==(const TDirectory& rhs) const
	{
		return IsSameInode(this->handle, rhs.handle);
	}

	iosize_t TDirectory::AproxCount() const
	{
		struct stat st = {};
		EL_SYSERR(fstat(this->handle, &st));
		return st.st_nlink;
	}

	/************************************************************************/

	usys_t TFile::Read(byte_t* const arr_items, const usys_t n_items_max)
	{
		const ssize_t n = EL_SYSERR(read(this->handle, arr_items, n_items_max));
		EL_ERROR(n < 0, TLogicException);
		return n;
	}

	usys_t TFile::Write(const byte_t* const arr_items, const usys_t n_items_max)
	{
		const ssize_t n = EL_SYSERR(write(this->handle, arr_items, n_items_max));
		EL_ERROR(n < 0, TLogicException);
		return n;
	}

	void TFile::Close()
	{
		handle.Close();
		access = TAccess::NONE;
	}

	iosize_t TFile::Size() const
	{
		struct stat st = {};
		EL_SYSERR(fstat(this->handle, &st));

		if((st.st_mode & S_IFMT) == S_IFIFO || (st.st_mode & S_IFMT) == S_IFCHR || (st.st_mode & S_IFMT) == S_IFSOCK)
			return NEG1;
		else
			return st.st_size;
	}

	bool TFile::operator==(const TFile& rhs) const
	{
		return IsSameInode(this->handle, rhs.handle);
	}

	u64_t TFile::ObjectID() const
	{
		struct stat st = {};
		EL_SYSERR(fstat(this->handle, &st));
		return st.st_ino;
	}

	iosize_t TFile::Offset() const
	{
		return EL_SYSERR(lseek(this->handle, 0, SEEK_CUR));
	}

	void TFile::Offset(const siosize_t position, const ESeekOrigin origin)
	{
		int w = 0;
		switch(origin)
		{
			case ESeekOrigin::START:
				w = SEEK_SET;
				break;
			case ESeekOrigin::END:
				w = SEEK_END;
				break;
			case ESeekOrigin::CURRENT:
				w = SEEK_CUR;
				break;
		}

		EL_SYSERR(lseek(this->handle, position, w));
	}

	TFile::TFile(THandle&& handle) : handle(std::move(handle))
	{
		// remove O_APPEND|O_DIRECT|O_NONBLOCK, add O_NOCTTY
		const int current_flags = EL_SYSERR(fcntl(this->handle, F_GETFL));
		const int new_flags = (current_flags | O_NOCTTY) & ~(O_APPEND|O_DIRECT|O_NONBLOCK);
		if(new_flags != current_flags)
			EL_SYSERR(fcntl(this->handle, F_SETFL, new_flags));

		if((current_flags & O_RDWR) == O_RDWR)
			this->access = TAccess::RW;
		else if((current_flags & O_RDONLY) == O_RDONLY)
			this->access = TAccess::RO;
		else if((current_flags & O_WRONLY) == O_WRONLY)
			this->access = TAccess::WO;
		else
			this->access = TAccess::NONE;
	}

	/************************************************************************/

	void TMapping::Remap(const iosize_t offset, const usys_t size)
	{
		const usys_t file_size = this->file->Size();

		// offset into the file rounded down to page boundary
		const usys_t map_offset = (offset / PAGE_SIZE) * PAGE_SIZE;

		// actual size of the mapping in bytes, NOT rounded to page size, kernel will round it up internally
		const usys_t map_size = (size == NEG1 ? file_size - offset : size) + (offset - map_offset);

		const int prot = (this->access.read ? PROT_READ : 0) | (this->access.write ? PROT_WRITE : 0) | (this->access.execute ? PROT_EXEC : 0);

		if(this->arr_items == nullptr)
		{
			EL_SYSERR(this->arr_items = (byte_t*)mmap(nullptr, map_size, prot, MAP_SHARED | MAP_FILE, this->file->Handle(), map_offset));
			this->arr_items += (offset - map_offset);
		}
		else
		{
			byte_t* arr_items_new;
			EL_SYSERR(arr_items_new = (byte_t*)mremap(this->arr_items - (this->offset - this->map_offset), this->map_size, map_size, MREMAP_MAYMOVE));
			this->arr_items = arr_items_new + (offset - map_offset);
		}

		this->map_offset = map_offset;
		this->map_size = map_size;
		this->n_items = map_size - (offset - map_offset);
	}

	TMapping::TMapping(TFile* const file, const iosize_t offset, const usys_t size) : array_t<byte_t>(), file(file), offset(offset), access(file->Access())
	{
		this->Remap(offset, size);
	}

	TMapping::TMapping(TFile* const file, const TAccess access, const iosize_t offset, const usys_t size) : array_t<byte_t>(), file(file), offset(offset), access(access)
	{
		this->Remap(offset, size);
	}

	TMapping::~TMapping()
	{
		if(this->arr_items != nullptr)
			munmap(this->arr_items, this->n_items);
	}
}

#endif
