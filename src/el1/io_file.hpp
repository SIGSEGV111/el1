#pragma once

#include "io_types.hpp"
#include "io_stream.hpp"
#include "io_collection_list.hpp"
#include "io_text_string.hpp"
#include "system_handle.hpp"
#include "system_time.hpp"
#include "util_function.hpp"

namespace el1::io::file
{
	using namespace io::types;
	using namespace io::stream;
	using namespace io::text::string;
	using namespace io::collection::list;
	using namespace system::handle;
	using namespace system::time;

	class TPath;
	class TDirectory;
	class TFile;

	enum class ESeekOrigin : u8_t
	{
		START,
		END,
		CURRENT
	};

	enum class ECreateMode : u8_t
	{
		OPEN,		// only open existing files, never create a new file
		NX,			// create missing files
		EXCLUSIVE,	// create a new file, fail if it already exists
		TRUNCATE,	// create new or open existing file, but truncate it to 0-bytes length
		DELETE,		// delete any existig file (or empty directory) and always create a new file
	};

	struct TAccess
	{
		bool read : 1,
			 write : 1,
			 execute : 1;

		static const TAccess NONE;	// read=false; write=false; execute=false
		static const TAccess RO;	// read=true;  write=false; execute=false
		static const TAccess WO;	// read=false; write=true;  execute=false
		static const TAccess RW;	// read=true;  write=true;  execute=false
		static const TAccess RX;	// read=true;  write=false; execute=true

		int FileOpenFlags() const EL_GETTER;
	};

	enum class EObjectType : u8_t
	{
		NX,				// does not exist
		UNKNOWN,		// only valid for TDirectory::Enum() (if the file-system did not report the type during enumeration)
		FILE,			// regular file
		DIRECTORY,		// directory
		CHAR_DEVICE,	// character device
		BLOCK_DEVICE,	// block device
		FIFO,			// pipe / fifo
		SYMLINK,		// symbolic link
		SOCKET			// unix socket
	};

	struct direntry_t
	{
		// ID of the file-system object
		// unique within the file-system it resides on
		u64_t obj_id;

		// ID of the file-system (same as fs_info_t::id)
		u64_t fs_id;

		TString name;

		iosize_t size;	// object size in bytes (this is the "apparent" or "virtual" size of the file-system object)
		iosize_t usage;	// effective disk-usage in bytes (this can be lower due to compression/holes or higher due to overhead)

		// type of object
		// can even be NX if the object vanished during enumeration
		EObjectType type;

		// number of hardlinks
		u32_t n_links;

		TTime ts_created;
		TTime ts_access;
		TTime ts_status;
		TTime ts_write;
	};

	class TPath
	{
		protected:
			TList<TString> components;
			mutable std::unique_ptr<char[]> cached_cstr;

			void Validate();

		public:
			void Simplify();

			bool IsEmpty() const { return components.Count() == 0; }

			// appends rhs to the current path
			// if rhs is an absolute path, this operation will fail
			TPath& operator+=(const TPath& rhs);
			TPath  operator+ (const TPath& rhs) const EL_GETTER;

			// removes rhs from the end of this path
			// if rhs is an absolute path, this operation will fail
			// if the last parts do  not match this operation will fail
			TPath& operator-=(const TPath& rhs);
			TPath  operator- (const TPath& rhs) const EL_GETTER;

			bool operator==(const TPath& rhs) const EL_GETTER;
			bool operator!=(const TPath& rhs) const EL_GETTER;

			// this function strips old_prefix from the beginning of this path and replaces it with new_prefix
			// if this path does not begin with old_prefix the function will fail
			// if this path is an absolute path, then old_prefix must also be an absolute path
			// new_prefix can be either and the resulting path will match that
			void Rebase(const TPath& old_prefix, const TPath& new_prefix);

			// returns only the name protion of the current file-system object with extensions (but no path)
			TString FullName() const EL_GETTER;

			// returns only the extension (the text behing the last '.' in the FullName)
			TString Extension() const EL_GETTER;

			// returns only the bare name without extension or path
			TString BareName() const EL_GETTER;

			// returns the path to the directory containing the current file-system object
			TPath Parent() const EL_GETTER;

			// returns a list containing the individual path components
			const array_t<const TString>& Components() const EL_GETTER { return this->components; }

			// allows to replace a path component with another path
			// if it is the first component beeing replaced then new_path can be an absolute path
			// in all other cases new_path it must be a relative path
			void ReplaceComponent(const ssys_t index, const TPath& new_path);

			// counts the common path components at the beginning
			usys_t CountCommonPrefix(const TPath& other) const;

			// strips away the common path components at the beginning
			// returns the number of components stripped
			usys_t StripCommonPrefix(const TPath& other);

			// modifies the current path to become relative to reference
			// this operation will fail if no common parent in the absolute paths can be found
			// NOTE: this function might return very different results depending on if you
			// call Resolve() beforehand on either this path, the reference path, or on both
			void MakeRelativeTo(const TPath& reference);

			// this function will convert this path to an absolute path
			// if this path already is an absolute path then this function has no effect
			void MakeAbsolute();

			// converts this path reference into a string usable by the operating system
			operator TString() const EL_GETTER;
			operator const char*() const EL_GETTER;
			TString ToString() const EL_GETTER { return this->operator TString(); }

			// returns the type of the file-system object
			// the result of this function is of volatile nature; another process could
			// modify (or delete) the file-system object in the meantime and also change its type
			// if the last component of this path is a symbolic link, then the operation is performed on the link itself, not on its target
			EObjectType Type() const EL_WARN_UNUSED_RESULT;

			// checks whether or not the file-system object represented by this path actually exists + if it is of a specific type
			// as with Type() this can change at any point in time
			bool Exists() const       EL_WARN_UNUSED_RESULT { return this->Type() != EObjectType::NX; }
			bool IsFile() const       EL_WARN_UNUSED_RESULT { return this->Type() == EObjectType::FILE; }
			bool IsDirectory() const  EL_WARN_UNUSED_RESULT { return this->Type() == EObjectType::DIRECTORY; }
			bool IsSymlink() const    EL_WARN_UNUSED_RESULT { return this->Type() == EObjectType::SYMLINK; }
			bool IsFifo() const       EL_WARN_UNUSED_RESULT { return this->Type() == EObjectType::FIFO; }
			bool IsSocket() const     EL_WARN_UNUSED_RESULT { return this->Type() == EObjectType::SOCKET; }

			// checks whether or not the path is a mount point or similar equivalent construct (windows drive root; NTFS mount point)
			// a mount point itself says nothing about the object-type involved - check Type() to find out what type it is!
			bool IsMountpoint() const EL_WARN_UNUSED_RESULT;

			// creates a file or directory at the location pointed to by this path, the file will be initially empty and with read/write access
			// if a file-system object already exists with the same path, these function will fail
			// if recursive is set to true, then any non-existant parent directory will be created (not atomic!)
			// if the last component of this path is a symbolic link, then the operation fails, quoting that the object already exists
			TFile CreateAsFile(const bool recursive) const;
			void CreateAsDirectory(const bool recursive) const;

			// Strip() removes path compontents at the beginning, while Truncate() cuts of path compontents at the end
			void Strip(const usys_t n_compontents);
			void Truncate(const usys_t n_compontents);

			// this function resolves all symbolic links and similar file-system indirections and transforms the path into an absolute canonical path
			// this operation is not guaranteed to be atomic and needs to access the file-system to perform this operation, in turn this will
			// required sufficient access permissions on the involved directories
			// as such it will fail if any required path component doesn't exist or access is denied
			void Resolve();

			// returns whether or not this path is an absolute path (this does not check for symbolic links)
			bool IsAbsolute() const EL_GETTER;
			bool IsRelative() const EL_GETTER { return !this->IsAbsolute(); }

			// checks whether or not the requested access rights are granted on the file-system object
			// as with Type() this can change at any point in time
			// if the file-system object does not exist, all access will be reported as denied
			// if the last component of this path is a symbolic link, then information on the link itself is provided
			bool HasAccess(const TAccess request) const;

			// moves the file-system object to a new location and/or renames it
			// if it is a directoy, all its content will be recursively moved along
			// this operation is often not atomic and can involve ("slow") copy and delete operations
			// if the operation requires to delete files from the source location, then
			// this will happen AFTER ALL files have been successfully copied to the new location
			// if a copy operation fails for any reason this function will abort and try to undo what it did so far
			// if the last component of this path is a symbolic link, then the link itself is renamed/moved, not its target
			// if overwrite is set to true and the target already exists, an attempt is made to recursively delete the target
			void Move(const TPath& new_path, const bool overwrite = false) const;

			// similar to Move(), but actually copies the file/directory, leaving the source intact
			void Copy(const TPath& new_path, const bool overwrite = false) const;

			// deletes the file-system object specified by this path
			// in case of a non-empty directory, you have to set recursive to true or else the call will fail
			void Delete(const bool recursive) const;

			// calls receiver for every file found in the path
			// match_type limits the files returned, if set to NX no filter is applied
			// unlike TDirectory::Enum() the fields in direntry_t always contain a valid as if QueryInfo() had been called
			// bool Receiver(const TPath& cwd, const direntry_t& e);
			u64_t Browse(util::function::TFunction<bool, const TPath&, const direntry_t&> receiver, const bool recursive = false, const EObjectType match_type = EObjectType::NX, const bool include_root = true) const;

			// creates a canonicalized path object from a native textual path representation
			// this strips away path components that have no effect (e.g. "./" or "xyz/../")
			// no verrification happens whether or not the resulting path actually exists
			// however paths that cannot exist by the rules of the operating system will be rejected
			TPath(const TString& str);
			TPath(const char* const str);
			TPath(const wchar_t* const str);

			TPath(std::initializer_list<TString> list);
// 			TPath(std::initializer_list<const char*> list);

			TPath() = default;
			TPath(TPath&&) = default;
			TPath(const TPath&);
			TPath& operator=(TPath&&) = default;
			TPath& operator=(const TPath&);

			static TPath CurrentWorkingDirectory();

			// the character used by the operating system to seperate path compontents (forward slash on posix and backslash on windows)
			static const TUTF32 SEPERATOR;

			// a relative path which indicates the parent directory (most operating systems use '..')
			static const char* const PARENT_DIR;

			// a relative path which indicates the current directory (most operating systems use '.')
			static const char* const CURRENT_DIR;
	};

	struct TInvalidPathException : error::IException
	{
		enum class EReason
		{
			EMPTY_COMPONENT,
			EXPECTED_RELATIVE_PATH,
			NO_MATCH,
			EMPTY_PATH,
			COMPONENT_CONTAINS_SEPERATOR,
			POINTS_BEFORE_ROOT,
		};

		static const char* ReasonText(const EReason reason);

		TPath path;
		usys_t idx_component;	// -1 if no specific component is at fault
		EReason reason;

		TString Message() const final override;
		IException* Clone() const override;

		TInvalidPathException(const TPath& path, const usys_t idx_component, const EReason reason) : path(path), idx_component(idx_component), reason(reason) {}
	};

	struct fs_info_t
	{
		// this is an implementation defined unique ID that can be used to
		// check if two file-system objects are on the same file-system
		// it might change between reboots, on remounts of the file-system,
		// or accross different version of el1 and thus should not be stored
		u64_t id;

		// name of the file-system type (e.g. ext4, btrfs, xfs, ntfs, fat, ...) as reported by the OS
		TString type;

		// please treat below numbers with care
		// some file-systems cannot reliably determine the
		// effective free space available to applications due to
		// redundancy (e.g. RAID1) or compression
		// values are in bytes
		iosize_t space_total;
		iosize_t space_free;
	};

	class TDirectory
	{
		protected:
			THandle handle;

		public:
			// do not alter the returned handle in any way!
			const THandle& Handle() const EL_GETTER { return this->handle; }

			// enumerates the content of the directory
			// this can be aborted by the receiver by returning false
			// only direntry_t::name is guaranteed to be valid
			// fields that have no valid value are set to -1
			void Enum(util::function::TFunction<bool, const direntry_t&> receiver) const;

			bool Contains(const TString& name) const;
			bool IsRoot() const;
			bool IsMountpoint() const;

			TDirectory Parent() const;

			// retrieves information about the named file-system object within this directory
			// this call usually yields more infomation than Enum()
			direntry_t QueryInfo(const TString& name) const;

			// returns an aproximated count of file-system objects within the directoy without
			// actually enumerating them - this might be inaccurate, but should give a rough idea
			iosize_t AproxCount() const;

			// compares if the two directories refer to the same underlying file-system object
			bool operator==(const TDirectory& rhs) const EL_WARN_UNUSED_RESULT;
			bool operator!=(const TDirectory& rhs) const EL_WARN_UNUSED_RESULT { return !(*this == rhs); }

			// returns the path of this directory as reported by the operating system
			// if the directory was deleted an empty path or valid alternate path is returned
			// this operation is not guaranteed to be atomic
			TPath Path() const;

			// returns information about the file-system on which this directory is stored
			fs_info_t FileSystem() const EL_WARN_UNUSED_RESULT;

			// returns the file-system wide unique identifier for this directory
			u64_t ObjectID() const EL_GETTER;

			// opens the directory pointed to by path
			// if path is a relative path it is interpreted relative to the threads current working directory
			// this will open a handle on the operating system which will track the directory
			// this *might* "lock" the directory and prevent other applications from renaming or
			// deleting the directory itself or any of its parents
			TDirectory(const TPath& path);

			// as above but with this variant the path is interpreted relative to base_dir (if it is actually a relative path)
			TDirectory(const TDirectory& base_dir, const TPath& path);

			TDirectory(const TDirectory&);
			TDirectory(TDirectory&&) = default;

			TDirectory& operator=(const TDirectory&);
			TDirectory& operator=(TDirectory&&) = default;
	};

	class TFile : public ISink<byte_t>, public ISource<byte_t>
	{
		protected:
			THandle handle;
			TAccess access;

		public:
			handle_t Handle() final override { return handle; }
			TAccess Access() const { return this->access; }

			usys_t Read(byte_t* const arr_items, const usys_t n_items_max) final override;
			usys_t Write(const byte_t* const arr_items, const usys_t n_items_max) final override;

			void Close() final override;

			// returns or sets the IO-offset into the file
			// the offset is automatically increased by the IO-routines above
			void Offset(const siosize_t position, const ESeekOrigin origin = ESeekOrigin::START);
			iosize_t Offset() const;

			// returns the size of the file in bytes
			// the result is volatile in nature, as other processes could be actively writing to the file
			iosize_t Size() const EL_WARN_UNUSED_RESULT;

			// compares if the two open files refer to the same underlying file-system object
			bool operator==(const TFile& rhs) const EL_WARN_UNUSED_RESULT;
			bool operator!=(const TFile& rhs) const EL_WARN_UNUSED_RESULT { return !(*this == rhs); }

			// returns the path of this file as reported by the operating system
			// if the file was deleted an empty path or a valid alternate path is returned
			// this operation is not guaranteed to be atomic
			TPath Path() const;

			// returns information about the file-system on which this directory is stored
			fs_info_t FileSystem() const EL_WARN_UNUSED_RESULT;

			// returns the file-system wide unique identifier for this file
			u64_t ObjectID() const EL_GETTER;

			// returns the handle used by this file object
			const THandle& Handle() const EL_GETTER { return handle; }

			// creates a temporary file
			// the file will be automatically deleted when the file object is detroyed
			// unless it was linked into the file-system
			TFile(); // uses the systems temporary file directory (uses TMPDIR environment variable if set)
			TFile(const TDirectory& temp_dir);

			// opens or creates a file
			TFile(const TPath& path, const TAccess access = TAccess::RO, const ECreateMode create = ECreateMode::OPEN);
			TFile(const TDirectory& dir, const TString& filename, const TAccess access = TAccess::RO, const ECreateMode create = ECreateMode::OPEN);

			// takes control over a open file handle
			explicit TFile(THandle&& handle);

			// creates a second TFile object which refers to the same underlying file
			// but will have its own independent IO-offset
			TFile(const TFile&) = default;

			TFile(TFile&&) = default;
			~TFile();

			/**
			* @brief Reads UTF8 encoded text from a file specified by the path.
			*
			* This static method reads the content of a UTF8 encoded text file into a TString object.
			* The method can optionally trim whitespace from the beginning and end of
			* the text and limit the amount of data read.
			*
			* @param path A TPath object representing the file path to read from.
			*
			* @param trim A boolean flag indicating whether to trim whitespace from
			*             the read text. Defaults to true.
			*
			* @param n_chars_max Sets the maximum number of characters (not bytes!) to be read from the file.
			*                    The default is 1024. If the file is larger than this, only the
			*                    first n_chars_max characters will be read. The limit is applied before
			*                    the whitespace is trimmed, hence it counts against the limit.
			*
			* @return Returns a TString containing the text read from the file. If the file
			*         cannot be read, an exception is thrown.
			*/
			static TString ReadText(const TPath& path, const bool trim = true, const usys_t n_chars_max = 1024U);
	};

	class TMapping : public array_t<byte_t>
	{
		protected:
			TFile* const file;
			iosize_t offset;
			iosize_t map_offset;
			usys_t map_size;
			const TAccess access;

		public:
			TAccess Access() const { return this->access; }

			// reconfigures the mapping, possibly changing its address in memory
			void Remap(const iosize_t offset, const usys_t size);

			// creates a virtual memory mapping of the specified file
			// the mapping starts at offset-bytes into the file (counting from the start) and extends size-bytes long
			// if size is -1 then the mapping will be as large as possible (respecting the TFile::Size() and any operating system imposed limits)
			// access only allows to further RESTRICT the access to the mapping, it cannot exeed the permissions granted by the underlying file
			// modifications made to the content of the mapping are carried through to the underlying file (with potentially some delay)
			// changes made by other processes through a mapping to the same region of the file immediately become visible in this mapping
			// the interactions with the low-level IO-routines (read/write) used by TFile are left unspecified
			TMapping(TFile* const file, const iosize_t offset = 0, const usys_t size = NEG1);
			TMapping(TFile* const file, const TAccess access, const iosize_t offset = 0, const usys_t size = NEG1);
			~TMapping();
	};
}
