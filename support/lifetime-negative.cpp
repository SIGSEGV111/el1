// Intentionally invalid lifetime patterns. `make lifetime-check` compiles this
// file expecting Clang to reject it. It is not part of the normal build.
#include <el1/io_collection_list.hpp>
#include <el1/io_file.hpp>
#include <el1/io_text_string.hpp>

using namespace el1;
using namespace el1::io::collection::array;
using namespace el1::io::collection::list;
using namespace el1::io::text::string;

TStringView ReturnLocalStringView()
{
    TString value(U"temporary");
    return value.View();
}

array_t<const int> ReturnLocalArrayView()
{
    TList<int> value{1, 2, 3};
    return value.View();
}

void StoreTemporaryOwnerViews()
{
    TStringView string_view = TString(U"temporary");
    array_t<const int> array_view = TList<int>{1, 2, 3};
    (void)string_view;
    (void)array_view;
}

auto ReturnPipeBorrowingLocalSource()
{
    io::file::TFile file("/tmp/el1-lifetime-check");
    return file.Pipe();
}
