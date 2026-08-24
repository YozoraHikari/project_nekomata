export module projnekomata:core.command.shell;
import std;
import projnekomata.cs;
import :core.command.cvar;


using CommandArgs = Vec<std::string>;

namespace projnekomata {

auto setCvar(CommandArgs args) -> void {
    if (args.size() < 2) {
        log::error("setcv: not enough arguments");
        return;
    }

    auto& cvarName = args[0];
    auto& cvarValue = args[1];

    auto cvar = CvarManager::get().getCvar(cvarName);
    if (cvar == nullptr) {
        log::error("setcv: cvar {} not found", cvarName);
        return;
    }

    cvar->setFromString(cvarValue);
    log::info("setcv: {} = {}", cvarName, cvarValue);
}

auto getCvar(CommandArgs args) -> void {
    if (args.size() < 1) {
        log::error("getcv: not enough arguments");
        return;
    }

    auto& cvarName = args[0];
    auto cvar = CvarManager::get().getCvar(cvarName);
    if (cvar == nullptr) {
        log::error("getcv: cvar {} not found", cvarName);
        return;
    }

    log::info("getcv: {} = {}", cvarName, cvar->toString());
}

struct CommandDescription {
    auto(*run)(CommandArgs args) -> void;
    std::string_view description;
};

static auto kCommands = HashMap<std::string, CommandDescription>::create({
    { "setcv", { setCvar, "set a cvar" } },
    { "getcv", { getCvar, "get a cvar" } },
});

}

export namespace projnekomata {

auto cmdRun(std::string_view cmd) -> void {
    auto cmdMnemonic = std::string(cmd.substr(0, cmd.find(' ')));
    auto cmdArgs = cmd.substr(cmdMnemonic.size());

    auto entry = kCommands.get(cmdMnemonic);
    if (entry.isNone()) {
        log::error("unknown command: {}", cmdMnemonic);
        return;
    }

    auto args = CommandArgs::create();
    std::string currentArg = "";

    auto i = 0_usize;
    auto isInQuotes = false;

    while (i < cmdArgs.size()) {
        auto c = cmdArgs[i];

        if (!isInQuotes) {
            if (c == ' ') {
                if (currentArg.empty()) {
                    i++;
                    continue;
                }
                args.push(std::move(currentArg));
                currentArg = "";
            } else if (c == '"') {
                isInQuotes = true;
            } else {
                currentArg.push_back(c);
            }
        } else {
            if (c == '\\' && i + 1 < cmdArgs.size()) {
                auto escapeChar = cmdArgs[i + 1];
                switch (escapeChar) {
                    case '\\': currentArg.push_back('\\'); break;
                    case 'n': currentArg.push_back('\n'); break;
                    case 't': currentArg.push_back('\t'); break;
                    case '"': currentArg.push_back('"'); break;
                    default: currentArg.push_back(escapeChar); break;
                }
                i++; // skip the escape character
            } else if (c == '"') {
                isInQuotes = false;
            } else {
                currentArg.push_back(c);
            }
        }

        i++;
    }
    args.push(std::move(currentArg));

    entry.unwrap().get().run(std::move(args));
}

}