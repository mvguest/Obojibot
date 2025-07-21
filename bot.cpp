#include <dpp/dpp.h>
#include <fstream>
#include <iostream>
#include <nlohmann/json.hpp>

std::string get_token(const std::string& filename) {
    std::ifstream file(filename);
    if (!file.is_open()) {
        throw std::runtime_error("ERROR: Not possible to open config.json");
    }

    nlohmann::json config;
    file >> config;

    if (!config.contains("TOKEN") || !config["TOKEN"].is_string()) {
        throw std::runtime_error("ERROR: No TOKEN found on config.json");
    }

    return config["TOKEN"];
}

int main() {
    std::string token;

    try {
        token = get_token("config.json");
    } catch (const std::exception& e) {
        std::cerr << "ERROR: " << e.what() << "\n";
        return 1;
    }

    dpp::cluster bot(token);

    bot.on_log(dpp::utility::cout_logger());

    bot.on_slashcommand([](const dpp::slashcommand_t& event) {
        if (event.command.get_command_name() == "ping") {
            event.reply("Pong!");
        }
    });

    bot.on_ready([&bot](const dpp::ready_t& event) {
        if (dpp::run_once<struct register_bot_commands>()) {
            bot.global_command_create(dpp::slashcommand("ping", "Responde com Pong!", bot.me.id));
        }
    });

    bot.start(dpp::st_wait);
}

