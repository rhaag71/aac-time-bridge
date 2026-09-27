// Copyright (c) 2026 Rob Haag
// SPDX-License-Identifier: MIT
#include "console/LineEditor.h"
#include "console/Commands.h"
#include "network/Configuration.h"
#include <assert.h>
#include <string>
#include <vector>
#include <stdio.h>
using namespace aac;
struct Terminal {
    LineEditor editor;
    std::string output;
    std::vector<std::string> commands;
    unsigned rejected = 0;
    void input(const std::string& bytes) {
        for (unsigned char c : bytes) {
            auto event = editor.accept(c);
            output += editor.echo();
            if (event == LineEditor::Event::Submit) {
                commands.push_back(editor.line());
                if (consoleCommand(editor.line()) == ConsoleCommand::Wifi) {
                    Configuration configuration;
                    if (!parseConfiguration(editor.line(), configuration)) output += kInvalidConsoleCommand;
                }
            } else if (event == LineEditor::Event::Reject) { ++rejected; output += kInvalidConsoleInput; }
            if (event != LineEditor::Event::None) editor.clearLine();
        }
    }
};
int main() {
    for (const char* ending : {"\r", "\n", "\r\n"}) {
        Terminal t; t.input(std::string("provision") + ending);
        assert(t.commands.size() == 1 && t.commands[0] == "provision");
        assert(t.output == "provision\r\n");
        t.input("factory reset\r\n"); assert(t.commands.size() == 2);
    }
    Terminal t;
    t.input("provisiom\b"); assert(t.output == "provisiom\b \b");
    t.input("nx\177\n"); assert(t.commands.size() == 1 && t.commands[0] == "provision");
    Terminal empty; empty.input("\b\177\r\n"); assert(empty.commands.empty());
    Terminal limit; limit.input(std::string(LineEditor::capacity - 1, 'x') + "\n");
    assert(limit.commands.size() == 1 && limit.commands[0].size() == 111);
    Terminal overflow; overflow.input(std::string(LineEditor::capacity, 'x') + "\b\b\r\n");
    assert(overflow.commands.empty() && overflow.rejected == 1);
    overflow.input("provision\n"); assert(overflow.commands.size() == 1);
    Terminal controls; controls.input(std::string("factory") + char(0) + " reset\n");
    controls.input("provision\033[A\n"); assert(controls.commands.empty() && controls.rejected == 2);
    Terminal secret; secret.input("wifi Lab network\tPRIVATE-secret9");
    assert(secret.output == "wifi Lab network ");
    secret.input("\b\177XY\r\n");
    assert(secret.output == "wifi Lab network \r\n");
    Configuration config;
    assert(parseConfiguration(secret.commands[0].c_str(), config));
    assert(!strcmp(config.password, "PRIVATE-secreXY"));
    assert(!*secret.editor.line());
    Terminal edit;
    edit.input("wifi Lab\tabc\b\b\b"); assert(edit.output == "wifi Lab ");
    edit.input("\b"); assert(edit.output == "wifi Lab \b \b");
    edit.input("X\tnew-secret\n"); assert(edit.commands[0] == "wifi LabX\tnew-secret");
    assert(edit.output.find("new-secret") == std::string::npos);
    Terminal invalidPassword; invalidPassword.input("wifi Lab\tSECRET\n");
    assert(invalidPassword.output.find("SECRET") == std::string::npos);
    assert(invalidPassword.output.find(kInvalidConsoleCommand) != std::string::npos);
    Terminal longPassword; longPassword.input("wifi Lab\t" + std::string(200, 'Q') + "\n");
    assert(longPassword.commands.empty() && longPassword.rejected == 1);
    assert(longPassword.output.find('Q') == std::string::npos);
    Terminal malformed; malformed.input("WIFI Lab\tNeverShow\n");
    assert(malformed.output == "WIFI Lab \r\n"); // Privacy also for invalid command spelling.
    assert(consoleCommand("factory reset") == ConsoleCommand::FactoryReset);
    assert(consoleCommand("FACTORY RESET") == ConsoleCommand::Unknown);
    assert(consoleCommand("provision") == ConsoleCommand::Provision);
    assert(consoleCommand("PROVISION") == ConsoleCommand::Unknown);
    assert(consoleCommand("WEDGE WATCHDOG") == ConsoleCommand::Unknown);
#ifdef AAC_WATCHDOG_BENCH
    assert(consoleCommand("wedge watchdog") == ConsoleCommand::WedgeWatchdog);
    puts("bench console parsing, editing and credential echo tests passed");
#else
    assert(consoleCommand("wedge watchdog") == ConsoleCommand::Unknown);
    puts("production console parsing, editing and credential echo tests passed");
#endif
}
