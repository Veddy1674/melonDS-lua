/*
    Copyright 2021-2023 melonDS team

    This file is part of melonDS.

    melonDS is free software: you can redistribute it and/or modify it under
    the terms of the GNU General Public License as published by the Free
    Software Foundation, either version 3 of the License, or (at your option)
    any later version.

    melonDS is distributed in the hope that it will be useful, but WITHOUT ANY
    WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
    FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.

    You should have received a copy of the GNU General Public License along
    with melonDS. If not, see http://www.gnu.org/licenses/.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <QApplication>
#include <QCommandLineParser>
#include <QStringList>

#include "CLI.h"
#include "Platform.h"

using melonDS::Platform::Log;
using melonDS::Platform::LogLevel;

namespace CLI
{

CommandLineOptions* ManageArgs(QApplication& melon, int argc, char** argv)
{
    QCommandLineParser parser;
    parser.addHelpOption();

    parser.addPositionalArgument("nds", "Nintendo DS ROM (or an archive file which contains it) to load into Slot-1");
    parser.addPositionalArgument("gba", "GBA ROM (or an archive file which contains it) to load into Slot-2");

    parser.addOption(QCommandLineOption({"b", "boot"}, "Whether to boot firmware on startup. Defaults to \"auto\" (boot if NDS rom given)", "auto/always/never", "auto"));
    parser.addOption(QCommandLineOption({"f", "fullscreen"}, "Start melonDS in fullscreen mode"));

    parser.addOption(QCommandLineOption("winsize", "Window size multiplier (1-4)", "size"));
    parser.addOption(QCommandLineOption("winx", "Window X position", "x"));
    parser.addOption(QCommandLineOption("winy", "Window Y position", "y"));
    parser.addOption(QCommandLineOption("nolimit", "Disable FPS limit"));

    parser.addOption(QCommandLineOption("lua", "Lua script to run on startup", "script"));
    // found out this might be the best clean alternative to pass args to lua
    parser.addOption(QCommandLineOption("lua-args", "Arguments to pass to Lua script", "args")); // "arg0|arg1|arg2"

#ifdef ARCHIVE_SUPPORT_ENABLED
    parser.addOption(QCommandLineOption({"a", "archive-file"}, "Specify file to load inside an archive given (NDS)", "rom"));
    parser.addOption(QCommandLineOption({"A", "archive-file-gba"}, "Specify file to load inside an archive given (GBA)", "rom"));
#endif

    parser.process(melon);

    CommandLineOptions* cliOptions = new CommandLineOptions;

    cliOptions->fullscreen = parser.isSet("fullscreen");

    QStringList posargs = parser.positionalArguments();
    switch (posargs.size())
    {
        default:
            Log(LogLevel::Warn, "Too many positional arguments; ignoring 3 onwards\n");
        case 2:
            cliOptions->gbaRomPath = posargs[1];
        case 1:
            cliOptions->dsRomPath = posargs[0];
        case 0:
            break;
    }

    QString bootMode = parser.value("boot");
    if (bootMode == "auto")
    {
        cliOptions->boot = !posargs.empty();
    }
    else if (bootMode == "always")
    {
        cliOptions->boot = true;
    }
    else if (bootMode == "never")
    {
        cliOptions->boot = false;
    }
    else
    {
        Log(LogLevel::Error, "ERROR: -b/--boot only accepts auto/always/never as arguments\n");
        exit(1);
    }

    if (parser.isSet("winsize"))
        cliOptions->winSize = parser.value("winsize").toInt();

    if (parser.isSet("winx"))
        cliOptions->winX = parser.value("winx").toInt();

    if (parser.isSet("winy"))
        cliOptions->winY = parser.value("winy").toInt();

    cliOptions->fpsLimit = !parser.isSet("nolimit"); // aka maxspeed

    if (parser.isSet("lua"))
        cliOptions->luaScript = parser.value("lua");

    cliOptions->luaArgs = {};
    if (parser.isSet("lua-args"))
    {
        QString argsStr = parser.value("lua-args");
        QStringList args = argsStr.split('|');
        for (const QString& arg : args)
            cliOptions->luaArgs.push_back(arg.toStdString());
    }

    // e.g: ./melonDS $ROM --lua $LUA --lua-args "arg0|arg1"

#ifdef ARCHIVE_SUPPORT_ENABLED
    if (parser.isSet("archive-file"))
    {
        if (cliOptions->dsRomPath.has_value())
        {
            cliOptions->dsRomArchivePath = parser.value("archive-file");
        }
        else
        {
            Log(LogLevel::Error, "Option -a/--archive-file given, but no archive specified!");
        }
    }

    if (parser.isSet("archive-file-gba"))
    {
        if (cliOptions->gbaRomPath.has_value())
        {
            cliOptions->gbaRomArchivePath = parser.value("archive-file-gba");
        }
        else
        {
            Log(LogLevel::Error, "Option -A/--archive-file-gba given, but no archive specified!");
        }
    }
#endif

    return cliOptions;
}

}
