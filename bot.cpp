#include <dpp/dpp.h>
#include <fstream>
#include <iostream>
#include <nlohmann/json.hpp>

#define JSON_INDENT 4

using json = nlohmann::json;
json inventories;
const std::string inventory_file = "db/inventories.json";

void load_inventory() {
    std::ifstream in(inventory_file);
    if (in.is_open()) {
        try {
            in >> inventories;
        } catch (const std::exception& e) {
            std::cerr << "ERROR-JSON (inventories.json): " << e.what() << "\n";
            inventories = json::object(); 
        }
    } else {
        inventories = json::object(); 
    }
}

void save_inventory() {
    std::ofstream out(inventory_file);
    out << inventories.dump(JSON_INDENT);
}

void add_item(const std::string& user_id, const std::string& item) {
    if (!inventories[user_id].contains(item) || !inventories[user_id][item].is_number()) {
        inventories[user_id][item] = 1;
    } else {
        inventories[user_id][item] = inventories[user_id][item].get<int>() + 1;
    }
    save_inventory();
}


std::string get_inventory(const std::string& user_id) {
    if (!inventories.contains(user_id) || inventories[user_id].empty())
        return "📭 Seu inventário está vazio.";

    std::string result = "📦 Seu inventário:\n";
    for (auto& [item, qty] : inventories[user_id].items()) {
        result += "- " + item + " x" + std::to_string(qty.get<int>()) + "\n";
    }
    return result;
}

std::string get_token(const std::string& filename) {
    std::ifstream file(filename);
    if (!file.is_open()) {
        throw std::runtime_error("ERROR: Not possible to open config.json");
    }

    json config;
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

    load_inventory();

    bot.on_log(dpp::utility::cout_logger());

    bot.on_slashcommand([&](const dpp::slashcommand_t& event) {
        std::string user_id = std::to_string(event.command.member.user_id);

        if (event.command.get_command_name() == "ping") {
            event.reply("Pong!");
        } else if (event.command.get_command_name() == "item_append") {
            std::string item = std::get<std::string>(event.get_parameter("item"));
            add_item(user_id, item);
            event.reply("✅ Item adicionado: " + item);
        } else if (event.command.get_command_name() == "inventory") {
            event.reply(get_inventory(user_id));
        }
    });

    bot.on_ready([&bot](const dpp::ready_t& event) {
        if (dpp::run_once<struct register_bot_commands>()) {
            bot.global_command_create(
                dpp::slashcommand("ping", "Responde com Pong!", bot.me.id)
            );
            bot.global_command_create(
                dpp::slashcommand("item_append", "Adiciona um item ao seu inventário", bot.me.id)
                    .add_option(dpp::command_option(dpp::co_string, "item", "Nome do item", true))
            );
            bot.global_command_create(
                dpp::slashcommand("inventory", "Mostra seu inventário", bot.me.id)
            );
        }
    });

    bot.start(dpp::st_wait);
}

