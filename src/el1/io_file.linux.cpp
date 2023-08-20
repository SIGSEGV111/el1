#include "def.hpp"
#ifdef EL_OS_LINUX

#include "io_file.hpp"
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/sysmacros.h>
#include <sys/vfs.h>    /* or <sys/statfs.h> */
#include "system_task.hpp"

namespace el1::io::file
{
	using namespace util::function;

	static const TPath SYSFS_FD_PATH("/proc/self/fd");

	static TPath ReadLink(const TPath& symlink)
	{
		TList<char> buffer;

		ssize_t r;
		do
		{
			buffer.FillInsert(-1, '\x00', 256);
			EL_SYSERR(r = ::readlink(symlink, &buffer[0], buffer.Count()));
		}
		while(r == (ssize_t)buffer.Count());

		return TString(&buffer[0], r);
	}

	static TPath GetHandlePath(const THandle& handle)
	{
		const TPath target = ReadLink(SYSFS_FD_PATH + TString::Format("%d", (int)handle));

		struct stat st1 = {};
		struct stat st2 = {};

		if(stat(target, &st1) == -1)
			return TPath();

		EL_SYSERR(fstat(handle, &st2));

		if(st1.st_ino == st2.st_ino && st1.st_dev == st2.st_dev)
			return target;

		return TPath();
	}

	struct magic_to_name_t
	{
		u32_t magic;
		char name[12];
	};

	// from the man-page STATFS(2)
	static const magic_to_name_t __MAP_MAGIC_TO_NAME[] = {
		{ 0x0000002f, "QNX4"        },
		{ 0x00000187, "AUTOFS"      },
		{ 0x00001373, "DEVFS"       },
		{ 0x0000137d, "EXT"         },
		{ 0x0000137f, "MINIX"       },
		{ 0x0000138f, "MINIX2"      },
		{ 0x00001cd1, "DEVPTS"      },
		{ 0x00002468, "MINIX2"      },
		{ 0x00002478, "MINIX22"     },
		{ 0x00003434, "NILFS"       },
		{ 0x00004244, "HFS"         },
		{ 0x00004d44, "MSDOS"       },
		{ 0x00004d5a, "MINIX3"      },
		{ 0x0000517b, "SMB"         },
		{ 0x0000564c, "NCP"         },
		{ 0x00006969, "NFS"         },
		{ 0x00007275, "ROMFS"       },
		{ 0x000072b6, "JFFS2"       },
		{ 0x00009660, "ISOFS"       },
		{ 0x00009fa0, "PROC"        },
		{ 0x00009fa1, "OPENPROM"    },
		{ 0x00009fa2, "USBDEVICE"   },
		{ 0x0000adf5, "ADFS"        },
		{ 0x0000adff, "AFFS"        },
		{ 0x0000ef51, "EXT2_OLD"    },
		{ 0x0000ef53, "EXT2/3/4"    },
		{ 0x0000f15f, "ECRYPTFS"    },
		{ 0x00011954, "UFS"         },
		{ 0x0027e0eb, "CGROUP"      },
		{ 0x00414a53, "EFS"         },
		{ 0x00c0ffee, "HOSTFS"      },
		{ 0x01021994, "TMPFS"       },
		{ 0x01021997, "V9FS"        },
		{ 0x012fd16d, "XIAFS"       },
		{ 0x012ff7b4, "XENIX"       },
		{ 0x012ff7b5, "SYSV4"       },
		{ 0x012ff7b6, "SYSV2"       },
		{ 0x012ff7b7, "COH"         },
		{ 0x09041934, "ANON_INODE"  },
		{ 0x0bad1dea, "FUTEXFS"     },
		{ 0x11307854, "MTD_INODE"   },
		{ 0x15013346, "UDF"         },
		{ 0x19800202, "MQUEUE"      },
		{ 0x1badface, "BFS"         },
		{ 0x28cd3d45, "CRAMFS"      },
		{ 0x3153464a, "JFS"         },
		{ 0x42465331, "BEFS"        },
		{ 0x42494e4d, "BINFMTFS"    },
		{ 0x43415d53, "SMACK"       },
		{ 0x50495045, "PIPEFS"      },
		{ 0x52654973, "REISERFS"    },
		{ 0x5346414f, "AFS"         },
		{ 0x5346544e, "NTFS"        },
		{ 0x534f434b, "SOCKFS"      },
		{ 0x58465342, "XFS"         },
		{ 0x6165676c, "PSTOREFS"    },
		{ 0x62646576, "BDEVFS"      },
		{ 0x62656572, "SYSFS"       },
		{ 0x63677270, "CGROUP2"     },
		{ 0x64626720, "DEBUGFS"     },
		{ 0x65735546, "FUSE"        },
		{ 0x68191122, "QNX6"        },
		{ 0x6e736673, "NSFS"        },
		{ 0x73636673, "SECURITYFS"  },
		{ 0x73717368, "SQUASHFS"    },
		{ 0x73727279, "BTRFS_TEST"  },
		{ 0x73757245, "CODA"        },
		{ 0x7461636f, "OCFS2"       },
		{ 0x74726163, "TRACEFS"     },
		{ 0x794c7630, "OVERLAYFS"   },
		{ 0x858458f6, "RAMFS"       },
		{ 0x9123683e, "BTRFS"       },
		{ 0x958458f6, "HUGETLBFS"   },
		{ 0xa501fcf5, "VXFS"        },
		{ 0xabba1974, "XENFS"       },
		{ 0xcafe4a11, "BPF"         },
		{ 0xde5e81e4, "EFIVARFS"    },
		{ 0xf2f52010, "F2FS"        },
		{ 0xf97cff8c, "SELINUX"     },
		{ 0xf995e849, "HPFS"        },
		{ 0xfe534d42, "SMB2"        },
		{ 0xff534d42, "CIFS"        },
	};

	static const array_t<const magic_to_name_t> MAP_MAGIC_TO_NAME(__MAP_MAGIC_TO_NAME, sizeof(__MAP_MAGIC_TO_NAME) / sizeof(__MAP_MAGIC_TO_NAME[0]));

	static TString MagicToName(const u32_t magic)
	{
		const usys_t index = MAP_MAGIC_TO_NAME.BinarySearch([magic](const magic_to_name_t& item) {
			if(item.magic > magic) return  1;
															if(item.magic < magic) return -1;
															return 0;
		});

		if(index == NEG1)
			return TString();
		else
			return MAP_MAGIC_TO_NAME[index].name;
	}

	static fs_info_t GetFileSystemInfo(const THandle& handle)
	{
		struct stat st = {};
		struct statfs fs = {};

		EL_SYSERR(fstat(handle, &st));
		EL_SYSERR(fstatfs(handle, &fs));

		fs_info_t fs_info = {
			.id = st.st_dev,
			.type = MagicToName(fs.f_type),
			.space_total = fs.f_blocks * 512U,
			.space_free = (getuid() == 0 ? fs.f_bfree : fs.f_bavail) * 512U
		};

		return fs_info;
	}

	// from man-page GETDENTS(2)
	struct linux_dirent64 {
		ino64_t        d_ino;    /* 64-bit inode number */
		off64_t        d_off;    /* 64-bit offset to next structure */
		unsigned short d_reclen; /* Size of this dirent */
		unsigned char  d_type;   /* File type */
		char           d_name[]; /* Filename (null-terminated) */
	};

	static EObjectType ObjectTypeFromMode(const mode_t mode)
	{
		if((mode & S_IFMT) == S_IFREG)
		{
			return EObjectType::FILE;
		}
		else if((mode & S_IFMT) == S_IFDIR)
		{
			return EObjectType::DIRECTORY;
		}
		else if((mode & S_IFMT) == S_IFLNK)
		{
			return EObjectType::SYMLINK;
		}
		else if((mode & S_IFMT) == S_IFIFO)
		{
			return EObjectType::FIFO;
		}
		else if((mode & S_IFMT) == S_IFCHR)
		{
			return EObjectType::CHAR_DEVICE;
		}
		else if((mode & S_IFMT) == S_IFBLK)
		{
			return EObjectType::BLOCK_DEVICE;
		}
		else if((mode & S_IFMT) == S_IFSOCK)
		{
			return EObjectType::SOCKET;
		}
		else
		{
			EL_THROW(TLogicException);
		}
	}

	EObjectType TPath::Type() const
	{
		struct stat st = {};
		const int rc = fstatat(AT_FDCWD, *this, &st, AT_EMPTY_PATH|AT_NO_AUTOMOUNT|AT_SYMLINK_NOFOLLOW);
		if(rc == -1)
		{
			if(errno == ENOENT)
				return EObjectType::NX;
			EL_THROW(TSyscallException, errno);
		}

		return ObjectTypeFromMode(st.st_mode);
	}

	void TPath::Resolve()
	{
		const THandle handle(EL_SYSERR(open(*this, O_PATH|O_CLOEXEC)), true);
		*this = GetHandlePath(handle);
	}

	/**************************************************************************/

	fs_info_t TDirectory::FileSystem() const
	{
		return GetFileSystemInfo(this->handle);
	}

	TPath TDirectory::Path() const
	{
		return GetHandlePath(this->handle);
	}

	static EObjectType GetObjectType(const linux_dirent64& entry)
	{
// 		const char d_type = *(reinterpret_cast<const char*>(&entry) + entry.d_reclen - 1);

		switch(entry.d_type)
		{
			case DT_UNKNOWN: return EObjectType::UNKNOWN;
			case DT_REG:     return EObjectType::FILE;
			case DT_DIR:     return EObjectType::DIRECTORY;
			case DT_LNK:     return EObjectType::SYMLINK;
			case DT_FIFO:    return EObjectType::FIFO;
			case DT_SOCK:    return EObjectType::SOCKET;
			case DT_CHR:     return EObjectType::CHAR_DEVICE;
			case DT_BLK:     return EObjectType::BLOCK_DEVICE;
			default:         return EObjectType::UNKNOWN;
		}
	}

	void TDirectory::Enum(TFunction<bool, const direntry_t&> receiver) const
	{
		TList<byte_t> buffer;
		buffer.FillInsert(-1, 0, 32 * 1024);

		EL_SYSERR(lseek(this->handle, 0, SEEK_SET));

		for(;;)
		{
			ssize_t r;
			if((r = getdents64(this->handle, (linux_dirent64*)&buffer[0], buffer.Count())) < 0)
			{
				if(errno == EINVAL)
				{
					buffer.FillInsert(-1, 0, 4096);
				}
				else
				{
					EL_THROW(TSyscallException, errno);
				}
			}
			else if(r > 0)
			{
				const byte_t* p = &buffer[0];
				const byte_t* end = p + r;

				while(p < end)
				{
					const linux_dirent64* const de = (const linux_dirent64*)p;
					EL_ERROR(p + util::Max<usys_t>(sizeof(linux_dirent64), de->d_reclen) > &buffer[-1], TLogicException);
					if(de->d_reclen < sizeof(linux_dirent64))
						break;

// 					printf("name = '%s'; reclen = %d; off = %ld; sizeof() = %zu\n", de->d_name, de->d_reclen, de->d_off, sizeof(linux_dirent64));

					if(!(
							(de->d_name[0] == '.' && de->d_name[1] == 0) ||
							(de->d_name[0] == '.' && de->d_name[1] == '.' && de->d_name[2] == 0)
						))
					{

						direntry_t e = {
							.obj_id = de->d_ino,
							.fs_id = (u64_t)-1,
							.name = de->d_name,
							.size = (iosize_t)-1,
							.usage = (iosize_t)-1,
							.type = GetObjectType(*de),
							.n_links = (u32_t)-1,
							.ts_created = -1,
							.ts_access = -1,
							.ts_status = -1,
							.ts_write = -1,
						};

						if(!receiver(e))
							break;
					}

					p += de->d_reclen;
				}
			}
			else
			{
				break;
			}
		}
	}

	direntry_t TDirectory::QueryInfo(const TString& name) const
	{
		direntry_t e = {
			.obj_id = (u64_t)-1,
			.fs_id = (u64_t)-1,
			.name = name,
			.size = (iosize_t)-1,
			.usage = (iosize_t)-1,
			.type = EObjectType::UNKNOWN,
			.n_links = (u32_t)-1,
			.ts_created = -1,
			.ts_access = -1,
			.ts_status = -1,
			.ts_write = -1,
		};

		struct statx st = {};
		const int r = statx(this->handle, name.MakeCStr().get(), AT_NO_AUTOMOUNT | AT_SYMLINK_NOFOLLOW | AT_STATX_DONT_SYNC, STATX_BASIC_STATS | STATX_BTIME, &st);
		if(r < 0)
		{
			if(errno == ENOENT)
			{
				e.type = EObjectType::NX;
			}
			else
			{
				EL_THROW(TSyscallException, errno);
			}
		}
		else
		{
			if(st.stx_mask & STATX_INO)
			{
				e.obj_id = st.stx_ino;
				e.fs_id = ::makedev(st.stx_dev_major, st.stx_dev_minor);
			}

			if(st.stx_mask & STATX_SIZE)
				e.size = st.stx_size;

			if(st.stx_mask & STATX_BLOCKS)
				e.usage = st.stx_blocks * 512U;

			if(st.stx_mask & STATX_TYPE)
				e.type = ObjectTypeFromMode(st.stx_mode);

			if(st.stx_mask & STATX_NLINK)
				e.n_links = st.stx_nlink;

			if(st.stx_mask & STATX_BTIME)
				e.ts_created = TTime(st.stx_btime.tv_sec, st.stx_btime.tv_nsec * 1000000000LL);

			if(st.stx_mask & STATX_ATIME)
				e.ts_access = TTime(st.stx_atime.tv_sec, st.stx_atime.tv_nsec * 1000000000LL);

			if(st.stx_mask & STATX_CTIME)
				e.ts_status = TTime(st.stx_ctime.tv_sec, st.stx_ctime.tv_nsec * 1000000000LL);

			if(st.stx_mask & STATX_MTIME)
				e.ts_write = TTime(st.stx_mtime.tv_sec, st.stx_mtime.tv_nsec * 1000000000LL);
		}

		return e;
	}

	TDirectory::TDirectory(const TPath& path) : handle(THandle(EL_SYSERR(open(path, O_RDONLY|O_CLOEXEC|O_DIRECTORY)), true))
	{
	}

	TDirectory::TDirectory(const TDirectory& base_dir, const TPath& path) : handle(THandle(EL_SYSERR(openat(base_dir.Handle(), path, O_RDONLY|O_CLOEXEC|O_DIRECTORY)), true))
	{
	}

	/**************************************************************************/

	static int AccessToFlags(const TAccess access)
	{
		if(access.read && access.write)
			return O_RDWR;
		if(access.read)
			return O_RDONLY;
		if(access.write)
			return O_WRONLY;
		return O_PATH;
	}

	static int CreateModeToFlags(const ECreateMode create)
	{
		switch(create)
		{
			case ECreateMode::OPEN:		 return 0;
			case ECreateMode::NX:		 return O_CREAT;
			case ECreateMode::EXCLUSIVE: return O_CREAT|O_EXCL;
			case ECreateMode::TRUNCATE:	 return O_CREAT|O_TRUNC;
			case ECreateMode::DELETE:	 return O_CREAT|O_EXCL;
		}

		EL_THROW(TInvalidArgumentException, "create", "valid values are: OPEN, NX, EXCLUSIVE, TRUNCATE, DELETE");
	}

	TFile::TFile() : access(TAccess::RW)
	{
		const TString* const tmpdir = system::task::EnvironmentVariables().Get("TMPDIR");
		if(tmpdir == nullptr)
			this->handle = THandle(EL_SYSERR(open("/tmp", O_RDWR|O_CLOEXEC|O_TMPFILE, 0666)), true);
		else
			this->handle = THandle(EL_SYSERR(open(tmpdir->MakeCStr().get(), O_RDWR|O_CLOEXEC|O_TMPFILE, 0666)), true);
	}

	TFile::TFile(const TDirectory& temp_dir) : access(TAccess::RW)
	{
		this->handle = THandle(EL_SYSERR(openat(temp_dir.Handle(), ".", O_RDWR|O_CLOEXEC|O_TMPFILE, 0666)), true);
	}

	TFile::TFile(const TPath& path, const TAccess access, const ECreateMode create) : access(access)
	{
		TPath abspath = path;
		abspath.MakeAbsolute();
		abspath.Simplify();

		try
		{
			if(create == ECreateMode::DELETE)
			{
				if(unlink(path) == -1 && errno != ENOENT)
					rmdir(path);
			}

			const int flags = AccessToFlags(access) | CreateModeToFlags(create) | O_NOCTTY | O_CLOEXEC;
			this->handle = THandle(EL_SYSERR(open(abspath, flags, 0666)), true);
		}
		catch(const error::IException& e)
		{
			EL_FORWARD(e, TException, TString::Format("error while opening file %q", abspath.operator TString()));
		}
	}

	TFile::TFile(const TDirectory& dir, const TString& filename, const TAccess access, const ECreateMode create) : access(access)
	{
		auto path = filename.MakeCStr();
		if(create == ECreateMode::DELETE)
		{
			if(unlinkat(dir.Handle(), path.get(), 0) == -1 && errno != ENOENT)
				unlinkat(dir.Handle(), path.get(), AT_REMOVEDIR);
		}

		const int flags = AccessToFlags(access) | CreateModeToFlags(create) | O_NOCTTY | O_CLOEXEC;
		this->handle = THandle(EL_SYSERR(openat(dir.Handle(), path.get(), flags, 0666)), true);
	}

	fs_info_t TFile::FileSystem() const
	{
		return GetFileSystemInfo(this->handle);
	}

	TPath TFile::Path() const
	{
		return GetHandlePath(this->handle);
	}

	TFile::~TFile()
	{
		// nothing to do
	}
}

#endif
