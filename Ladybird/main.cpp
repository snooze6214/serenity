/*
 * Copyright (c) 2022, Andreas Kling <kling@serenityos.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include "BrowserWindow.h"
#include "HelperProcess.h"
#include "Settings.h"
#include "Utilities.h"
#include "WebContentView.h"
#include <AK/OwnPtr.h>
#include <Browser/CookieJar.h>
#include <Browser/Database.h>
#include <LibCore/ArgsParser.h>
#include <LibCore/EventLoop.h>
#include <LibCore/System.h>
#include <LibFileSystem/FileSystem.h>
#include <LibGfx/Font/FontDatabase.h>
#include <LibMain/Main.h>
#include <LibSQL/SQLClient.h>
#include <QApplication>

AK::OwnPtr<Browser::Settings> s_settings;

static ErrorOr<void> handle_attached_debugger()
{
#ifdef AK_OS_LINUX
    // Let's ignore SIGINT if we're being debugged because GDB
    // incorrectly forwards the signal to us even when it's set to
    // "nopass". See https://sourceware.org/bugzilla/show_bug.cgi?id=9425
    // for details.
    auto unbuffered_status_file = TRY(Core::File::open("/proc/self/status"sv, Core::File::OpenMode::Read));
    auto status_file = TRY(Core::BufferedFile::create(move(unbuffered_status_file)));
    auto buffer = TRY(ByteBuffer::create_uninitialized(4096));
    while (TRY(status_file->can_read_line())) {
        auto line = TRY(status_file->read_line(buffer));
        auto const parts = line.split_view(':');
        if (parts.size() < 2 || parts[0] != "TracerPid"sv)
            continue;
        auto tracer_pid = parts[1].to_uint<u32>();
        if (tracer_pid != 0UL) {
            dbgln("Debugger is attached, ignoring SIGINT");
            TRY(Core::System::signal(SIGINT, SIG_IGN));
        }
        break;
    }
#endif
    return {};
}

ErrorOr<int> serenity_main(Main::Arguments arguments)
{
    // NOTE: This is only used for the Core::Socket inside the IPC connections.
    // FIXME: Refactor things so we can get rid of this somehow.
    Core::EventLoop event_loop;

    TRY(handle_attached_debugger());

    QApplication app(arguments.argc, arguments.argv);

    platform_init();

    // NOTE: We only instantiate this to ensure that Gfx::FontDatabase has its default queries initialized.
    Gfx::FontDatabase::set_default_font_query("Katica 10 400 0");
    Gfx::FontDatabase::set_fixed_width_font_query("Csilla 10 400 0");

    StringView raw_url;
    StringView webdriver_content_ipc_path;
    bool enable_callgrind_profiling = false;

    Core::ArgsParser args_parser;
    args_parser.set_general_help("The Ladybird web browser :^)");
    args_parser.add_positional_argument(raw_url, "URL to open", "url", Core::ArgsParser::Required::No);
    args_parser.add_option(webdriver_content_ipc_path, "Path to WebDriver IPC for WebContent", "webdriver-content-path", 0, "path");
    args_parser.add_option(enable_callgrind_profiling, "Enable Callgrind profiling", "enable-callgrind-profiling", 'P');
    args_parser.parse(arguments);

    auto get_formatted_url = [&](StringView const& raw_url) -> ErrorOr<URL> {
        URL url = raw_url;
        if (FileSystem::exists(raw_url))
            url = URL::create_with_file_scheme(TRY(FileSystem::real_path(raw_url)).to_deprecated_string());
        else if (!url.is_valid())
            url = DeprecatedString::formatted("http://{}", raw_url);
        return url;
    };

    auto sql_server_paths = TRY(get_paths_for_helper_process("SQLServer"sv));
    auto sql_client = TRY(SQL::SQLClient::launch_server_and_create_client(move(sql_server_paths)));
    auto database = TRY(Browser::Database::create(move(sql_client)));

    auto cookie_jar = TRY(Browser::CookieJar::create(*database));

    s_settings = adopt_own_if_nonnull(new Browser::Settings());
    BrowserWindow window(cookie_jar, webdriver_content_ipc_path, enable_callgrind_profiling ? WebView::EnableCallgrindProfiling::Yes : WebView::EnableCallgrindProfiling::No);
    window.setWindowTitle("Ladybird");
    window.resize(800, 600);
    window.show();

    if (auto url = TRY(get_formatted_url(raw_url)); url.is_valid()) {
        window.view().load(url);
    } else if (!s_settings->homepage().isEmpty()) {
        auto home_url = TRY(ak_string_from_qstring(s_settings->homepage()));
        window.view().load(TRY(get_formatted_url(home_url.bytes_as_string_view())));
    }

    return app.exec();
}
