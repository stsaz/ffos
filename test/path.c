/** ffos: path.h tester
2020, Simon Zolin
*/

#include <FFOS/path.h>
#include <FFOS/test.h>

#define STR2(s)  (char*)(s), FFS_LEN(s)

void test_path_split()
{
	ffstr nm, ext;
	ffpath_splitname(STR2("file.txt"), &nm, &ext);
	xseq(&nm, "file");
	xseq(&ext, "txt");

	ffpath_splitname(STR2("qwer"), &nm, &ext);
	xseq(&nm, "qwer");
	xseq(&ext, "");

	ffpath_splitname(STR2(".qwer"), &nm, &ext);
	xseq(&nm, ".qwer");
	xseq(&ext, "");

	ffstr dir;
	x(FFS_LEN("/path/to") == ffpath_splitpath(STR2("/path/to/file"), &dir, &nm));
	xseq(&dir, "/path/to");
	xseq(&nm, "file");

	x(-1 == ffpath_splitpath(STR2("file"), &dir, &nm));
	xseq(&dir, "");
	xseq(&nm, "file");
}

void test_path_normalize()
{
	char buf[32];
	ffstr s;
	s.ptr = buf;

	// normal
	s.len = ffpath_normalize(s.ptr, sizeof(buf), STR2("a/b"), 0);
	xseq(&s, "a/b");
	s.len = ffpath_normalize(s.ptr, sizeof(buf), STR2("/a/b/"), 0);
	xseq(&s, "/a/b/");

	// merge slashes
	s.len = ffpath_normalize(s.ptr, sizeof(buf), STR2("a//b"), 0);
	xseq(&s, "a/b");
	s.len = ffpath_normalize(s.ptr, sizeof(buf), STR2("a//b//"), 0);
	xseq(&s, "a/b/");
	s.len = ffpath_normalize(s.ptr, sizeof(buf), STR2("//a\\\\b//c"), FFPATH_SLASH_BACKSLASH | FFPATH_FORCE_SLASH);
	xseq(&s, "/a/b/c");

	// .
	s.len = ffpath_normalize(s.ptr, sizeof(buf), STR2("./a/b"), 0);
	xseq(&s, "./a/b");
	s.len = ffpath_normalize(s.ptr, sizeof(buf), STR2("./a/./b"), 0);
	xseq(&s, "./a/b");

	// ..
	s.len = ffpath_normalize(s.ptr, sizeof(buf), STR2("../a/b"), 0);
	xseq(&s, "../a/b");
	s.len = ffpath_normalize(s.ptr, sizeof(buf), STR2("../../a"), 0);
	xseq(&s, "../../a");
	s.len = ffpath_normalize(s.ptr, sizeof(buf), STR2("a/../b"), 0);
	xseq(&s, "b");
	s.len = ffpath_normalize(s.ptr, sizeof(buf), STR2("./a/../../b"), 0);
	xseq(&s, "../b");
	s.len = ffpath_normalize(s.ptr, sizeof(buf), STR2("../a/../../b"), 0);
	xseq(&s, "../../b");
	s.len = ffpath_normalize(s.ptr, sizeof(buf), STR2("/a/../../b"), 0);
	xseq(&s, "/b");
	s.len = ffpath_normalize(s.ptr, sizeof(buf), STR2("c:/../../b"), 0);
#ifdef FF_WIN
	xseq(&s, "c:/b");
#else
	xseq(&s, "../b");
#endif
	s.len = ffpath_normalize(s.ptr, sizeof(buf), STR2("c:/../../b"), FFPATH_DISK_LETTER);
	xseq(&s, "c:/b");
	s.len = ffpath_normalize(s.ptr, sizeof(buf), STR2("c:/../../b"), FFPATH_NO_DISK_LETTER);
	xseq(&s, "../b");

	// slash
	s.len = ffpath_normalize(s.ptr, sizeof(buf), STR2("a\\b/c"), FFPATH_FORCE_SLASH);
#ifdef FF_WIN
	xseq(&s, "a/b/c");
#else
	xseq(&s, "a\\b/c");
#endif
	s.len = ffpath_normalize(s.ptr, sizeof(buf), STR2("a\\b/c"), FFPATH_SLASH_ONLY | FFPATH_FORCE_SLASH);
	xseq(&s, "a\\b/c");
	s.len = ffpath_normalize(s.ptr, sizeof(buf), STR2("a\\b/c"), FFPATH_SLASH_BACKSLASH | FFPATH_FORCE_SLASH);
	xseq(&s, "a/b/c");

	// simple
	s.len = ffpath_normalize(s.ptr, sizeof(buf), STR2("/"), FFPATH_SIMPLE);
	xseq(&s, "");
	s.len = ffpath_normalize(s.ptr, sizeof(buf), STR2("./a/./b"), FFPATH_SIMPLE);
	xseq(&s, "a/b");
	s.len = ffpath_normalize(s.ptr, sizeof(buf), STR2("./a/../../b"), FFPATH_SIMPLE);
	xseq(&s, "b");
	s.len = ffpath_normalize(s.ptr, sizeof(buf), STR2("../a/../b"), FFPATH_SIMPLE);
	xseq(&s, "b");
	s.len = ffpath_normalize(s.ptr, sizeof(buf), STR2("../a/../../b"), FFPATH_SIMPLE);
	xseq(&s, "b");
	s.len = ffpath_normalize(s.ptr, sizeof(buf), STR2("/a/../../b"), FFPATH_SIMPLE);
	xseq(&s, "b");
	s.len = ffpath_normalize(s.ptr, sizeof(buf), STR2("/a/b"), FFPATH_SIMPLE);
	xseq(&s, "a/b");
	s.len = ffpath_normalize(s.ptr, sizeof(buf), STR2("c:/a/b"), FFPATH_SIMPLE);
#ifdef FF_WIN
	xseq(&s, "a/b");
#else
	xseq(&s, "c:/a/b");
#endif
	s.len = ffpath_normalize(s.ptr, sizeof(buf), STR2("c:/a/b"), FFPATH_SIMPLE | FFPATH_DISK_LETTER);
	xseq(&s, "a/b");
}

void test_path()
{
	test_path_split();
	test_path_normalize();
}
