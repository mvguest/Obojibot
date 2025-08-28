#include <dpp/dpp.h>
#include <fstream>
#include <iostream>
#include <string>
#include <nlohmann/json.hpp>
#include <cstdlib>
#include <curl/curl.h>

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

json get_config(const std::string& filename) {
    std::ifstream file(filename);
    if (!file.is_open()) {
        throw std::runtime_error("ERROR: Not possible to open config.json");
    }

    json config;
    file >> config;
    return config;
}

static size_t write_callback(void* contents, size_t size, size_t nmemb, void* userp) {
    ((std::string*)userp)->append((char*)contents, size * nmemb);
    return size * nmemb;
}

std::string read_file(const std::string& filename) {
    std::ifstream file(filename, std::ios::binary);
    if (!file.is_open()) return "";
    return std::string((std::istreambuf_iterator<char>(file)),
                       (std::istreambuf_iterator<char>()));
}

std::string ask_ai(const std::string& question, const std::string& apiKey) {
    if (apiKey.empty()) return "❌ GEMINI_API_KEY não configurada!";

    std::string knowledge = read_file("res/Obojima.txt");
    std::string prompt = "Base de conhecimento:\n" + knowledge + "\n\nPergunta do usuário:\n" + question;

    CURL* curl = curl_easy_init();
    if (!curl) return "❌ Falha ao inicializar CURL.";

    std::string readBuffer;
    const std::string url = "https://generativelanguage.googleapis.com/v1beta/models/gemini-2.0-flash:generateContent";

    json requestBody = {
        {"contents", {{
            {"parts", {{
                {"text", prompt}
            }}}
        }}}
    };

    std::string requestStr = requestBody.dump();

    struct curl_slist* headers = NULL;
    headers = curl_slist_append(headers, "Content-Type: application/json");
    headers = curl_slist_append(headers, ("X-goog-api-key: " + apiKey).c_str());

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, requestStr.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &readBuffer);

    CURLcode res = curl_easy_perform(curl);
    curl_easy_cleanup(curl);
    curl_slist_free_all(headers);

    if (res != CURLE_OK) {
        return "❌ Erro na requisição: " + std::string(curl_easy_strerror(res));
    }

    try {
        json response = json::parse(readBuffer);
        return response["candidates"][0]["content"]["parts"][0]["text"];
    } catch (std::exception& e) {
        return "❌ Erro ao parsear JSON da IA: " + std::string(e.what());
    }
}

int main() {
    json config;
    try {
        config = get_config("config.json");
    } catch (const std::exception& e) {
        std::cerr << "ERROR: " << e.what() << "\n";
        return 1;
    }

    std::string token = config["TOKEN"];
    std::string api_key = config["GEMINI_API_KEY"];

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
        } else if (event.command.get_command_name() == "obojichat") {
            std::string message = std::get<std::string>(event.get_parameter("message"));
            std::string reply = ask_ai(message, api_key);
            event.reply(reply);
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
            bot.global_command_create(
                dpp::slashcommand("obojichat", "Converse com a IA especialista em Obojima", bot.me.id)
                    .add_option(dpp::command_option(dpp::co_string, "message", "Mensagem para a IA", true))
            );
        }
    });

    bot.start(dpp::st_wait);
}

