#include "llm/openai_backend.hpp"

#include <cstdlib>
#include <iostream>
#include <memory>


int main(){

    const char* key = std::getenv("NINE_ROUTER_API_KEY");
    if(key == nullptr){
        std::cerr << "Error: NINE_ROUTER_API_KEY environment variable is not set." << std::endl;
        return 1;
    }

    std::unique_ptr<LLMBackend> backend = std::make_unique<OpenAIBackend>(
        "https://hamm.up.railway.app/v1", "mistral/ministral-3b-latest", key);

    std::vector<Message> history = {
        {"system", "Kamu asisten pribadi yang menjawab singkat dalam bahasa Indonesia.", ""},
        {"user", "Halo, siapa kamu?", ""},
    };

    try {
        LLMResponse r = backend->chat(history, nlohmann::json::array());
        std::cout << "[" << backend->name() << "] " << r.text << '\n';
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << '\n';
        return 1;
    }

    return 0;
}