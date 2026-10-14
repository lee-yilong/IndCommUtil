// =========================================================================
// doctest 实现 + main 入口
//
// 关键：
//   - wxInitializer 初始化 wxBase
//   - wxTranslations::Set() 提供空翻译系统，使 _() 宏能安全调用
//     （无 catalog 时 _() 返回原文，不崩）
// =========================================================================

#define DOCTEST_CONFIG_IMPLEMENT
#include <wx/init.h>
#include <wx/translation.h>

#include "doctest.h"

int main(int argc, char** argv) {
    wxInitializer wxInit;
    if (!wxInit.IsOk()) {
        return 1;
    }

    wxTranslations::Set(new wxTranslations());  // 接管所有权，退出时自动删

    doctest::Context ctx;
    ctx.applyCommandLine(argc, argv);
    return ctx.run();
}
