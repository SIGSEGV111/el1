#include <gtest/gtest.h>
#include <el1/io_file.hpp>
#include <string.h>
#include <fcntl.h>

using namespace ::testing;

namespace
{
	using namespace el1::io::file;
	using namespace el1::error;

	static const TPath NX_TEST_PATH1 = TString("dir1") + TPath::SEPERATOR + TString("subdir1") + TPath::SEPERATOR + TString("file1.ext");
	static const TPath NX_TEST_PATH2 = TString("dir2") + TPath::SEPERATOR + TString("subdir2") + TPath::SEPERATOR + TString("file2.ext");

	TEST(io_file, TFile_Tempfile)
	{
		TFile file;
		EXPECT_EQ(file.Size(), 0U);
		EXPECT_EQ(file.Offset(), 0U);

		file.Write((const byte_t*)"hello world\n", 12);
		EXPECT_EQ(file.Size(), 12U);
		EXPECT_EQ(file.Offset(), 12U);

		file.Offset(6U);
		EXPECT_EQ(file.Size(), 12U);
		EXPECT_EQ(file.Offset(), 6U);

		char buffer[8] = {};
		EXPECT_EQ(file.Read((byte_t*)buffer, 8U), 6U);
		EXPECT_EQ(file.Size(), 12U);
		EXPECT_EQ(file.Offset(), 12U);
		EXPECT_TRUE(strcmp(buffer, "world\n") == 0);
	}

	/*
	TEST(io_file, TFile_Compare)
	{
		TFile f1("testfile1.txt", TAccess::RW, ECreateMode::DELETE);
		TFile f2("testfile1.txt", TAccess::RW, ECreateMode::OPEN);
		TFile f3("testfile2.txt", TAccess::RW, ECreateMode::DELETE);
		EXPECT_EQ(f1, f2);
		EXPECT_NE(f1, f3);
		EXPECT_NE(f2, f3);
	}
	*/

	TEST(io_file, TDirectory_Construct)
	{
		#ifdef EL_OS_LINUX
		{
			TDirectory dir1 = TPath("gen/test.tmp");
			TDirectory dir2 = TPath("gen/test.tmp");
			TDirectory dir3 = TPath(".");
			EXPECT_EQ(dir1, dir2);
			EXPECT_NE(dir1, dir3);
			EXPECT_NE(dir2, dir3);
		}
		#endif
	}

	TEST(io_file, TDirectory_Enum)
	{
		#ifdef EL_OS_LINUX
		{
			TList<direntry_t> dents;
			TDirectory testdata_dir = TPath("gen/testdata");
			testdata_dir.Enum([&](const direntry_t& de) { dents.Append(de); return true; });
			EXPECT_EQ(dents.Count(), 11U);

			auto comp_by_name = [](const char* const name, const direntry_t& de) {
				return de.name == name;
			};

			EXPECT_EQ(dents.Count(), 11U);

			EXPECT_TRUE(dents.Contains("dead-symlink", comp_by_name));
			EXPECT_TRUE(dents.Contains("dir", comp_by_name));
			EXPECT_TRUE(dents.Contains("dir-symlink", comp_by_name));
			EXPECT_TRUE(dents.Contains("empty-file", comp_by_name));
			EXPECT_TRUE(dents.Contains("fifo", comp_by_name));
			EXPECT_TRUE(dents.Contains("good-symlink", comp_by_name));
			EXPECT_TRUE(dents.Contains("hardlink.txt", comp_by_name));
			EXPECT_TRUE(dents.Contains("hw.txt", comp_by_name));
			EXPECT_TRUE(dents.Contains("socket", comp_by_name));
			EXPECT_TRUE(dents.Contains("test1.json", comp_by_name));
		}
		#endif
	}

	TEST(io_file, TMapping_Construct)
	{
		const char* const str = "hello world!";
		const unsigned str_len = strlen(str);
		TFile tmpfile;
		tmpfile.Write((const byte_t*)str, str_len);

		{
			TMapping mapping(&tmpfile);
			EXPECT_EQ(mapping.Count(), str_len);
			EXPECT_TRUE(memcmp(str, &mapping[0], str_len) == 0);
		}

		{
			const unsigned offset = 3;
			TMapping mapping(&tmpfile, offset);
			EXPECT_EQ(mapping.Count(), str_len - offset);
			EXPECT_TRUE(memcmp(str + offset, &mapping[0], str_len - offset) == 0);
		}

		{
			const unsigned cutoff = 3;
			TMapping mapping(&tmpfile, 0, str_len - cutoff);
			EXPECT_EQ(mapping.Count(), str_len - cutoff);
			EXPECT_TRUE(memcmp(str, &mapping[0], str_len - cutoff) == 0);
		}

		{
			const unsigned offset = 3;
			const unsigned cutoff = 3;
			TMapping mapping(&tmpfile, offset, str_len - cutoff - offset);
			EXPECT_EQ(mapping.Count(), str_len - cutoff - offset);
			EXPECT_TRUE(memcmp(str + offset, &mapping[0], str_len - cutoff - offset) == 0);
		}
	}

	TEST(io_file, TFile_FileSystem)
	{
		TFile file;
		const auto fs = file.FileSystem();
		EXPECT_TRUE(fs.type.Length() > 0);
		EXPECT_NE(fs.id, 0U);
		EXPECT_NE(fs.space_total, 0U);
		EXPECT_NE(fs.space_free, 0U);
	}

	TEST(io_file, TFile_Offset)
	{
		{
			TFile file;
			file.Offset(0, ESeekOrigin::START);
			EXPECT_EQ(file.Offset(), 0U);
			file.Write((const byte_t*)"hello world\n", 12U);
			EXPECT_EQ(file.Offset(), 12U);
			file.Offset(0, ESeekOrigin::START);
			EXPECT_EQ(file.Offset(), 0U);
			file.Offset(0, ESeekOrigin::END);
			EXPECT_EQ(file.Offset(), 12U);
			file.Offset(0, ESeekOrigin::CURRENT);
			EXPECT_EQ(file.Offset(), 12U);
			file.Offset(-6, ESeekOrigin::CURRENT);
			EXPECT_EQ(file.Offset(), 6U);
			file.Offset(-2, ESeekOrigin::END);
			EXPECT_EQ(file.Offset(), 10U);
		}
	}

	TEST(io_file, TPath_Construct)
	{
		{
			TPath path = NX_TEST_PATH1;
			EXPECT_EQ(path.Components().Count(), 3U);
			EXPECT_EQ(path.Components()[0], "dir1");
			EXPECT_EQ(path.Components()[1], "subdir1");
			EXPECT_EQ(path.Components()[2], "file1.ext");
			EXPECT_FALSE(path.IsAbsolute());
			EXPECT_TRUE(path.IsRelative());
		}

		{
			TPath path = TString("file.ext");
			EXPECT_EQ(path.Components().Count(), 1U);
			EXPECT_EQ(path.Components()[0], "file.ext");
			EXPECT_FALSE(path.IsAbsolute());
			EXPECT_TRUE(path.IsRelative());
		}

		{
			TPath path = "./test/../././abc/..";
			EXPECT_EQ(path.Components().Count(), 7U);
			EXPECT_FALSE(path.IsAbsolute());
			EXPECT_TRUE(path.IsRelative());
			path.Simplify();
			EXPECT_EQ(path.Components().Count(), 0U);
			EXPECT_FALSE(path.IsAbsolute());
			EXPECT_TRUE(path.IsRelative());
		}

		{
			EXPECT_THROW(TPath(TString("foo") + TPath::SEPERATOR + TPath::SEPERATOR + "bar"), TInvalidPathException);
		}

		{
			TPath path = NX_TEST_PATH1;
			EXPECT_EQ(path, NX_TEST_PATH1);
			EXPECT_NE(path, NX_TEST_PATH2);
			path = NX_TEST_PATH2;
			EXPECT_EQ(path, NX_TEST_PATH2);
			EXPECT_NE(path, NX_TEST_PATH1);
		}
	}

	TEST(io_file, TPath_Name)
	{
		{
			TPath path = NX_TEST_PATH1;
			EXPECT_EQ(path.FullName(), "file1.ext");
			EXPECT_EQ(path.BareName(), "file1");
			EXPECT_EQ(path.Extension(), "ext");
			EXPECT_EQ(path.Parent().FullName(), "subdir1");
			EXPECT_EQ(path.Parent().Parent().FullName(), "dir1");
		}

		{
			const TPath path = { "test.abc.ext" };
			EXPECT_EQ(path.BareName(), "test.abc");
			EXPECT_EQ(path.FullName(), "test.abc.ext");
			EXPECT_EQ(path.Extension(), "ext");
		}

		{
			const TPath path = { "test" };
			EXPECT_EQ(path.BareName(), "test");
			EXPECT_EQ(path.FullName(), "test");
			EXPECT_EQ(path.Extension(), "");
		}

		{
			const TPath path = { "test.long-ext" };
			EXPECT_EQ(path.BareName(), "test");
			EXPECT_EQ(path.FullName(), "test.long-ext");
			EXPECT_EQ(path.Extension(), "long-ext");
		}
	}

	TEST(io_file, TPath_Type)
	{
		{
			TPath path = "non-existing-file.txt";
			EXPECT_EQ(path.Type(), EObjectType::NX);
		}

		{
			TPath path = TPath::CurrentWorkingDirectory() + "gen" + "testdata";
			EXPECT_EQ(path.Type(), EObjectType::DIRECTORY);
			EXPECT_FALSE(path.IsMountpoint());
		}

		{
			TPath path = "Makefile";
			EXPECT_EQ(path.Type(), EObjectType::FILE);
		}

		#ifdef EL_OS_LINUX
		{
			EXPECT_EQ(TPath("gen/testdata/hw.txt"      ).Type(), EObjectType::FILE);
			EXPECT_EQ(TPath("gen/testdata/empty-file"  ).Type(), EObjectType::FILE);
			EXPECT_EQ(TPath("gen/testdata/hardlink.txt").Type(), EObjectType::FILE);
			EXPECT_EQ(TPath("gen/testdata/dead-symlink").Type(), EObjectType::SYMLINK);
			EXPECT_EQ(TPath("gen/testdata/good-symlink").Type(), EObjectType::SYMLINK);
			EXPECT_EQ(TPath("gen/testdata/dir-symlink" ).Type(), EObjectType::SYMLINK);
			EXPECT_EQ(TPath("gen/testdata/dir"         ).Type(), EObjectType::DIRECTORY);
			EXPECT_EQ(TPath("gen/testdata/socket"      ).Type(), EObjectType::SOCKET);
			EXPECT_EQ(TPath("gen/testdata/fifo"        ).Type(), EObjectType::FIFO);
			EXPECT_EQ(TPath("/dev/null").Type(), EObjectType::CHAR_DEVICE);
		}
		#endif
	}

	/*
	TEST(io_file, TPath_CreateAsDirectory)
	{
		{
			TPath path = TString("new-dir") + TPath::SEPERATOR + "sub-dir1";
			EXPECT_EQ(path.Type(), EObjectType::NX);
			path.CreateAsDirectory(true);
			EXPECT_EQ(path.Type(), EObjectType::DIRECTORY);
			TPath check = TString("new-dir") + TPath::SEPERATOR + "sub-dir1";
			EXPECT_EQ(check.Type(), EObjectType::DIRECTORY);
			path.CreateAsDirectory(true);
		}

		{
			TPath path = TString("new-dir") + TPath::SEPERATOR + "sub-dir2";
			EXPECT_EQ(path.Type(), EObjectType::NX);
			path.CreateAsDirectory(false);
			EXPECT_EQ(path.Type(), EObjectType::DIRECTORY);
			TPath check = TString("new-dir") + TPath::SEPERATOR + "sub-dir2";
			EXPECT_EQ(check.Type(), EObjectType::DIRECTORY);
		}

		{
			TPath path = TString("non-existing-dir") + TPath::SEPERATOR + "sub-dir";
			EXPECT_EQ(path.Type(), EObjectType::NX);
			EXPECT_THROW(path.CreateAsDirectory(false), TSyscallException);
			EXPECT_EQ(path.Type(), EObjectType::NX);
		}

		{
			TPath path = TString("non-existing-dir") + TPath::SEPERATOR + "sub-dir";
			EXPECT_EQ(path.Type(), EObjectType::NX);
			path.CreateAsDirectory(true);
			EXPECT_EQ(path.Type(), EObjectType::DIRECTORY);
		}
	}
	*/

	TEST(io_file, TPath_Rebase)
	{
		{
			TPath path = NX_TEST_PATH1;
			EXPECT_EQ(path.Components()[0], "dir1");
			path.Rebase("dir1", "dir2");
			EXPECT_EQ(path.Components()[0], "dir2");
			EXPECT_EQ(path.Components()[1], "subdir1");
			EXPECT_EQ(path.Components()[2], "file1.ext");
		}
	}

	TEST(io_file, TPath_AddSubstract)
	{
		{
			TPath path1 = "foo";
			TPath path2 = "bar";
			TPath path3 = path1 + path2;
			EXPECT_EQ(path3.Components()[0], "foo");
			EXPECT_EQ(path3.Components()[1], "bar");
			EXPECT_EQ(path3.Components().Count(), 2U);
		}

		{
			TPath path1 = TString("foo") + TPath::SEPERATOR + "bar";
			TPath path2 = "bar";
			TPath path3 = path1 - path2;
			EXPECT_EQ(path3.Components()[0], "foo");
			EXPECT_EQ(path3.Components().Count(), 1U);
		}
	}

	TEST(io_file, TPath_ReplaceComponent)
	{
		{
			TPath path = NX_TEST_PATH1;
			path.ReplaceComponent(0, NX_TEST_PATH2);
			EXPECT_EQ(path.Components().Count(), 5U);
			EXPECT_EQ(path.Components()[0], "dir2");
			EXPECT_EQ(path.Components()[1], "subdir2");
			EXPECT_EQ(path.Components()[2], "file2.ext");
			EXPECT_EQ(path.Components()[3], "subdir1");
			EXPECT_EQ(path.Components()[4], "file1.ext");
		}
	}


	TEST(io_file, TPath_StripCommonPrefix)
	{
		{
			TPath path = NX_TEST_PATH1;
			EXPECT_EQ(path.StripCommonPrefix(NX_TEST_PATH2), 0U);
			EXPECT_EQ(path.Components().Count(), 3U);
			EXPECT_EQ(path.Components()[0], "dir1");
			EXPECT_EQ(path.Components()[1], "subdir1");
			EXPECT_EQ(path.Components()[2], "file1.ext");
		}

		{
			TPath path = NX_TEST_PATH1 + "test";
			EXPECT_EQ(path.StripCommonPrefix(NX_TEST_PATH1), 3U);
			EXPECT_EQ(path.Components().Count(), 1U);
			EXPECT_EQ(path.Components()[0], "test");
		}
	}

	TEST(io_file, TPath_MakeRelativeTo)
	{
		{
			TPath path = NX_TEST_PATH1;
			path.MakeRelativeTo(NX_TEST_PATH2);
			EXPECT_EQ(path.Components().Count(), 6U);
			EXPECT_EQ(path.Components()[0], TPath::PARENT_DIR);
			EXPECT_EQ(path.Components()[1], TPath::PARENT_DIR);
			EXPECT_EQ(path.Components()[2], TPath::PARENT_DIR);
			EXPECT_EQ(path.Components()[3], NX_TEST_PATH1.Components()[0]);
			EXPECT_EQ(path.Components()[4], NX_TEST_PATH1.Components()[1]);
			EXPECT_EQ(path.Components()[5], NX_TEST_PATH1.Components()[2]);
		}

		{
			TPath path1 = NX_TEST_PATH1 + "foo" + "bar";
			TPath path2 = NX_TEST_PATH1 + "abc" + "xyz";
			path1.MakeRelativeTo(path2);
			EXPECT_EQ(path1.Components().Count(), 4U);
			EXPECT_EQ(path1.Components()[0], TPath::PARENT_DIR);
			EXPECT_EQ(path1.Components()[1], TPath::PARENT_DIR);
			EXPECT_EQ(path1.Components()[2], "foo");
			EXPECT_EQ(path1.Components()[3], "bar");
		}
	}

	/*
	TEST(io_file, TPath_CreateAsFile)
	{
		const TPath path = TString("foo") + TPath::SEPERATOR + "bar" + TPath::SEPERATOR + "file.txt";
		EXPECT_EQ(path.Type(), EObjectType::NX);
		EXPECT_EQ(path.Parent().Type(), EObjectType::NX);
		EXPECT_THROW(path.CreateAsFile(false), TException);
		TFile file = path.CreateAsFile(true);
		file.Write((const byte_t*)"hello world\n", 12U);
		EXPECT_EQ(path.Type(), EObjectType::FILE);
		EXPECT_EQ(path.Parent().Type(), EObjectType::DIRECTORY);
	}
	*/

	TEST(io_file, TPath_Strip)
	{
		{
			TPath path = { "foo", "bar", "baz" };
			EXPECT_EQ(path.Components().Count(), 3U);
			path.Strip(1);
			EXPECT_EQ(path.Components().Count(), 2U);
			EXPECT_EQ(path.Components()[0], "bar");
			EXPECT_EQ(path.Components()[1], "baz");
		}
	}

	TEST(io_file, TPath_Simplify)
	{
		{
			TPath test = {"some", "path", "..", "file"};
			const TPath expect = {"some", "file"};

			test.Simplify();
			EXPECT_EQ(test, expect);
		}

		{
			TPath test = {"some", "path", "..", "..", "file"};
			const TPath expect = {"file"};

			test.Simplify();
			EXPECT_EQ(test, expect);
		}

		{
			TPath test = {"some", "path", "..", "..", "..", "file"};
			const TPath expect = {"..", "file"};

			test.Simplify();
			EXPECT_EQ(test, expect);
		}

		{
			TPath test = {".", "some", "..", "path", "..", ".", "file"};
			const TPath expect = {"file"};

			test.Simplify();
			EXPECT_EQ(test, expect);
		}

		{
			TPath test = TPath::CurrentWorkingDirectory();
			const usys_t n = test.Components().Count();
			for(usys_t i = 0; i < n + 1; i++)
				test += "..";
			test += "file.txt";

			EXPECT_THROW(test.Simplify(), TInvalidPathException);
		}
	}

	TEST(io_file, TAccess_FileOpenFlags)
	{
		EXPECT_EQ(TAccess::NONE.FileOpenFlags(), 0);
		EXPECT_EQ(TAccess::RO.FileOpenFlags(), O_RDONLY);
		EXPECT_EQ(TAccess::WO.FileOpenFlags(), O_WRONLY);
		EXPECT_EQ(TAccess::RW.FileOpenFlags(), O_RDWR);
	}

	TEST(io_file, TPath_EdgeCases)
	{
		const TPath empty;
		EXPECT_EQ(empty.ToString(), ".");
		EXPECT_EQ(empty.Parent(), TPath(".."));

		const TPath root("/");
		EXPECT_TRUE(root.IsAbsolute());
		EXPECT_EQ(root.Parent(), root);
		EXPECT_TRUE(root.IsMountpoint());

		const TPath a("alpha/beta/gamma");
		const TPath b("alpha/beta/delta");
		EXPECT_EQ(a.CountCommonPrefix(b), 2U);
		EXPECT_EQ(a.CountCommonPrefix(TPath("other")), 0U);
	}

	TEST(io_file, TPath_CreateDeleteAndDirectoryInfo)
	{
		const TPath root("gen/test1.tmp/io-file-tree");
		if(root.Exists())
			root.Delete(true);

		const TPath nested = root + "a" + "b";
		nested.CreateAsDirectory(true);
		ASSERT_TRUE(nested.IsDirectory());

		const TPath file_path = nested + "sample.txt";
		{
			TFile file = file_path.CreateAsFile(false);
			const char* const text = "  alpha beta  \n";
			EXPECT_EQ(file.Write(reinterpret_cast<const byte_t*>(text), strlen(text)), strlen(text));
		}

		EXPECT_TRUE(file_path.IsFile());
		EXPECT_EQ(TFile::ReadText(file_path), "alpha beta");
		EXPECT_EQ(TFile::ReadText(file_path, false), "  alpha beta  \n");
		EXPECT_EQ(TFile::ReadText(file_path, true, 7), "alpha");

		TDirectory directory(nested);
		EXPECT_TRUE(directory.Contains("sample.txt"));
		EXPECT_FALSE(directory.Contains("missing.txt"));
		EXPECT_GT(directory.ObjectID(), 0U);
		EXPECT_GT(directory.AproxCount(), 0U);
		EXPECT_EQ(directory.Path().FullName(), "b");
		EXPECT_EQ(directory.Parent(), TDirectory(nested.Parent()));

		const direntry_t file_info = directory.QueryInfo("sample.txt");
		EXPECT_EQ(file_info.type, EObjectType::FILE);
		EXPECT_EQ(file_info.size, strlen("  alpha beta  \n"));
		EXPECT_GT(file_info.obj_id, 0U);

		const direntry_t missing_info = directory.QueryInfo("missing.txt");
		EXPECT_EQ(missing_info.type, EObjectType::NX);

		TPath resolved = file_path;
		resolved.Resolve();
		EXPECT_TRUE(resolved.IsAbsolute());
		EXPECT_EQ(resolved.FullName(), "sample.txt");

		root.Delete(true);
		EXPECT_EQ(root.Type(), EObjectType::NX);
	}

	TEST(io_file, TFile_CreateModesIdentityAndMappingRemap)
	{
		const TPath root("gen/test1.tmp/io-file-modes");
		if(root.Exists())
			root.Delete(true);
		root.CreateAsDirectory(true);
		const TPath path = root + "data.bin";

		{
			TFile file(path, TAccess::RW, ECreateMode::EXCLUSIVE);
			const byte_t data[] = { 0, 1, 2, 3, 4, 5, 6, 7 };
			EXPECT_EQ(file.Write(data, sizeof(data)), sizeof(data));
			EXPECT_THROW(TFile(path, TAccess::RW, ECreateMode::EXCLUSIVE), TException);
		}

		{
			TFile first(path, TAccess::RW, ECreateMode::NX);
			TFile second(path, TAccess::RO, ECreateMode::OPEN);
			EXPECT_EQ(first, second);
			EXPECT_EQ(first.ObjectID(), second.ObjectID());
			EXPECT_EQ(first.Path().FullName(), "data.bin");
			EXPECT_EQ(first.Size(), 8U);

			TMapping mapping(&first, 0, 4);
			EXPECT_EQ(mapping.Count(), 4U);
			mapping.Remap(0, 8);
			ASSERT_EQ(mapping.Count(), 8U);
			for(usys_t i = 0; i < mapping.Count(); i++)
				EXPECT_EQ(mapping[i], i);
		}

		{
			TFile file(path, TAccess::RW, ECreateMode::TRUNCATE);
			EXPECT_EQ(file.Size(), 0U);
			EXPECT_TRUE(file.Access().read);
			EXPECT_TRUE(file.Access().write);
			file.Close();
			EXPECT_FALSE(file.Access().read);
			EXPECT_FALSE(file.Access().write);
		}

		{
			TFile file(path, TAccess::RW, ECreateMode::DELETE);
			EXPECT_EQ(file.Size(), 0U);
		}

		root.Delete(true);
	}

	TEST(io_file, TDirectory_Root)
	{
		const TDirectory root("/");
		EXPECT_TRUE(root.IsRoot());
		EXPECT_EQ(root.Parent(), root);
		EXPECT_GT(root.ObjectID(), 0U);
	}

}
