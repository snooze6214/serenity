/*
 * Copyright (c) 2020, the SerenityOS developers.
 * Copyright (c) 2023, Sam Atkins <atkinssj@serenityos.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include "ChessWidget.h"
#include <LibConfig/Client.h>
#include <LibCore/System.h>
#include <LibDesktop/Launcher.h>
#include <LibFileSystemAccessClient/Client.h>
#include <LibGUI/ActionGroup.h>
#include <LibGUI/Application.h>
#include <LibGUI/Clipboard.h>
#include <LibGUI/Icon.h>
#include <LibGUI/Menu.h>
#include <LibGUI/Menubar.h>
#include <LibGUI/MessageBox.h>
#include <LibGUI/Process.h>
#include <LibGUI/Window.h>
#include <LibMain/Main.h>

ErrorOr<int> serenity_main(Main::Arguments arguments)
{
    TRY(Core::System::pledge("stdio rpath recvfd sendfd thread proc exec unix"));

    auto app = TRY(GUI::Application::try_create(arguments));

    Config::pledge_domain("Games");
    Config::monitor_domain("Games");

    TRY(Desktop::Launcher::add_allowed_handler_with_only_specific_urls("/bin/Help", { URL::create_with_file_scheme("/usr/share/man/man6/Chess.md") }));
    TRY(Desktop::Launcher::seal_allowlist());

    auto app_icon = TRY(GUI::Icon::try_create_default_icon("app-chess"sv));

    auto window = TRY(GUI::Window::try_create());
    auto widget = TRY(window->set_main_widget<ChessWidget>());

    TRY(Core::System::unveil("/res", "r"));
    TRY(Core::System::unveil("/bin/ChessEngine", "x"));
    TRY(Core::System::unveil("/bin/GamesSettings", "x"));
    TRY(Core::System::unveil("/tmp/session/%sid/portal/launch", "rw"));
    TRY(Core::System::unveil("/tmp/session/%sid/portal/filesystemaccess", "rw"));
    TRY(Core::System::unveil(nullptr, nullptr));

    window->set_title("Chess");
    window->set_base_size({ 4, 4 });
    window->set_size_increment({ 8, 8 });
    window->resize(508, 508);

    window->set_icon(app_icon.bitmap_for_size(16));

    widget->set_piece_set(Config::read_string("Games"sv, "Chess"sv, "PieceSet"sv, "stelar7"sv));
    widget->set_board_theme(Config::read_string("Games"sv, "Chess"sv, "BoardTheme"sv, "Beige"sv));
    widget->set_coordinates(Config::read_bool("Games"sv, "Chess"sv, "ShowCoordinates"sv, true));
    widget->set_show_available_moves(Config::read_bool("Games"sv, "Chess"sv, "ShowAvailableMoves"sv, true));

    auto game_menu = TRY(window->try_add_menu("&Game"_short_string));

    TRY(game_menu->try_add_action(GUI::Action::create("&Resign", { Mod_None, Key_F3 }, [&](auto&) {
        widget->resign();
    })));
    TRY(game_menu->try_add_action(GUI::Action::create("&Flip Board", { Mod_Ctrl, Key_F }, [&](auto&) {
        widget->flip_board();
    })));
    TRY(game_menu->try_add_separator());

    TRY(game_menu->try_add_action(GUI::Action::create("&Import PGN...", { Mod_Ctrl, Key_O }, [&](auto&) {
        auto result = FileSystemAccessClient::Client::the().open_file(window);
        if (result.is_error())
            return;

        if (auto maybe_error = widget->import_pgn(*result.value().release_stream()); maybe_error.is_error())
            dbgln("Failed to import PGN: {}", maybe_error.release_error());
        else
            dbgln("Imported PGN file from {}", result.value().filename());
    })));
    TRY(game_menu->try_add_action(GUI::Action::create("&Export PGN...", { Mod_Ctrl, Key_S }, [&](auto&) {
        auto result = FileSystemAccessClient::Client::the().save_file(window, "Untitled", "pgn");
        if (result.is_error())
            return;

        if (auto maybe_error = widget->export_pgn(*result.value().release_stream()); maybe_error.is_error())
            dbgln("Failed to export PGN: {}", maybe_error.release_error());
        else
            dbgln("Exported PGN file to {}", result.value().filename());
    })));
    TRY(game_menu->try_add_action(GUI::Action::create("&Copy FEN", { Mod_Ctrl, Key_C }, [&](auto&) {
        GUI::Clipboard::the().set_data(widget->get_fen().bytes());
        GUI::MessageBox::show(window, "Board state copied to clipboard as FEN."sv, "Copy FEN"sv, GUI::MessageBox::Type::Information);
    })));
    TRY(game_menu->try_add_separator());

    TRY(game_menu->try_add_action(GUI::Action::create("&New Game", { Mod_None, Key_F2 }, TRY(Gfx::Bitmap::load_from_file("/res/icons/16x16/reload.png"sv)), [&](auto&) {
        if (widget->board().game_result() == Chess::Board::Result::NotFinished) {
            if (widget->resign() < 0)
                return;
        }
        widget->reset();
    })));
    TRY(game_menu->try_add_separator());

    auto settings_action = GUI::Action::create(
        "Chess &Settings", {}, TRY(Gfx::Bitmap::load_from_file("/res/icons/16x16/games.png"sv)), [window](auto&) {
            GUI::Process::spawn_or_show_error(window, "/bin/GamesSettings"sv, Array { "--open-tab", "chess" });
        },
        window);
    settings_action->set_status_tip("Open the Game Settings for Chess");
    TRY(game_menu->try_add_action(settings_action));

    auto show_available_moves_action = GUI::Action::create_checkable("Show Available Moves", [&](auto& action) {
        widget->set_show_available_moves(action.is_checked());
        widget->update();
        Config::write_bool("Games"sv, "Chess"sv, "ShowAvailableMoves"sv, action.is_checked());
    });
    show_available_moves_action->set_checked(widget->show_available_moves());
    TRY(game_menu->try_add_action(show_available_moves_action));
    TRY(game_menu->try_add_separator());

    TRY(game_menu->try_add_action(GUI::CommonActions::make_quit_action([](auto&) {
        GUI::Application::the()->quit();
    })));

    auto engine_menu = TRY(window->try_add_menu("&Engine"_short_string));

    GUI::ActionGroup engines_action_group;
    engines_action_group.set_exclusive(true);
    auto engine_submenu = TRY(engine_menu->try_add_submenu("&Engine"_short_string));
    for (auto const& engine : { "Human", "ChessEngine" }) {
        auto action = GUI::Action::create_checkable(engine, [&](auto& action) {
            if (action.text() == "Human") {
                widget->set_engine(nullptr);
            } else {
                widget->set_engine(Engine::construct(action.text()));
                widget->input_engine_move();
            }
        });
        engines_action_group.add_action(*action);
        if (engine == DeprecatedString("Human"))
            action->set_checked(true);

        TRY(engine_submenu->try_add_action(*action));
    }

    auto help_menu = TRY(window->try_add_menu("&Help"_short_string));
    TRY(help_menu->try_add_action(GUI::CommonActions::make_command_palette_action(window)));
    TRY(help_menu->try_add_action(GUI::CommonActions::make_help_action([](auto&) {
        Desktop::Launcher::open(URL::create_with_file_scheme("/usr/share/man/man6/Chess.md"), "/bin/Help");
    })));
    TRY(help_menu->try_add_action(GUI::CommonActions::make_about_action("Chess", app_icon, window)));

    window->show();
    widget->reset();

    return app->exec();
}
