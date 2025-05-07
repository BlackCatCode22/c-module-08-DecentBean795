#include <iostream>
#include <string>
#include <vector>
#include <chrono>
#include <thread>
#include <ctime>
#include <curl/curl.h>
#include <nlohmann/json.hpp>

using json = nlohmann::json;
using namespace std;

size_t WriteCallback(void* contents, size_t size, size_t nmemb, string* out) {
    size_t totalSize = size * nmemb;
    out->append((char*)contents, totalSize);
    return totalSize;
}

string sendMessageToChatbot(const string& userMessage, const string& apiKey, int maxRetries = 3, int retryDelayMs = 1000) {
    string responseString;
    int attempt = 0;
    while (attempt < maxRetries) {
        CURL* curl = curl_easy_init();

        if (curl) {
            string url = "https://api.openai.com/v1/chat/completions";
            string payload = R"({
                "model": "gpt-4.1",
                "messages": [{"role": "user", "content": ")" + userMessage + R"("}]
            })";

            struct curl_slist* headers = nullptr;
            headers = curl_slist_append(headers, ("Authorization: Bearer " + apiKey).c_str());
            headers = curl_slist_append(headers, "Content-Type: application/json");

            curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
            curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
            curl_easy_setopt(curl, CURLOPT_POSTFIELDS, payload.c_str());
            curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
            curl_easy_setopt(curl, CURLOPT_WRITEDATA, &responseString);

            CURLcode res = curl_easy_perform(curl);
            curl_easy_cleanup(curl);
            curl_slist_free_all(headers);

            if (res == CURLE_OK && !responseString.empty()) {
                return responseString;
            } else {
                cerr << "Attempt " << (attempt + 1) << " failed. Retrying...\n";
                this_thread::sleep_for(chrono::milliseconds(retryDelayMs));
                attempt++;
            }
        }
    }

    return "Error: Failed to connect to the API after " + to_string(maxRetries) + " attempts.";
}

int main() {
    const char* apiKeyEnv = getenv("OPENAI_API_KEY");
    if (apiKeyEnv == nullptr) {
        cerr << "Error: OPENAI_API_KEY not set. Please set it in your environment variables." << endl;
        return 1;
    }
    string apiKey = apiKeyEnv;

    string userMessage;
    vector<pair<string, string>> conversationHistory;
    int chatCount = 0;
    double totalResponseTime = 0.0;

    cout << "Chatbot (type 'exit' to quit):\n";
    while (true) {
        cout << "> ";
        getline(cin, userMessage);

        if (userMessage == "exit") break;
        if (userMessage.empty()) {
            cout << "Error: Please enter a valid message.\n";
            continue;
        }
        if (userMessage.length() > 500) {
            cout << "Error: Message too long. Please enter a shorter message.\n";
            continue;
        }

        auto start = chrono::high_resolution_clock::now();
        string response = sendMessageToChatbot(userMessage, apiKey);
        auto end = chrono::high_resolution_clock::now();

        double responseTime = chrono::duration<double>(end - start).count();
        totalResponseTime += responseTime;
        chatCount++;

        try {
            json jsonResponse = json::parse(response);
            if (jsonResponse.contains("error")) {
                string errorMessage = jsonResponse["error"]["message"];
                cout << "Bot Error: " << errorMessage << "\n";
            } else if (jsonResponse.contains("choices") && !jsonResponse["choices"].empty()) {
                string botReply = jsonResponse["choices"][0]["message"]["content"];
                cout << "Bot: " << botReply << "\n";
                cout << "Response Tokens: " << jsonResponse["usage"]["completion_tokens"] << "\n";
                cout << "Prompt Tokens: " << jsonResponse["usage"]["prompt_tokens"] << "\n";
                cout << "Total Tokens: " << jsonResponse["usage"]["total_tokens"] << "\n";
                conversationHistory.push_back({"User", userMessage});
                conversationHistory.push_back({"Bot", botReply});
            } else {
                cout << "Bot: Sorry, I didn't understand the response. Please try again.\n";
            }
        } catch (const json::exception& e) {
            cout << "Bot: JSON parsing error: " << e.what() << "\n";
        }

        cout << "\nChat Statistics:\n";
        cout << "- Total Conversations: " << chatCount << "\n";
        cout << "- Average Response Time: " << (totalResponseTime / chatCount) << " seconds\n";
        cout << "\nConversation History:\n";
        for (const auto& pair : conversationHistory) {
            time_t now = time(0);
            cout << "[" << ctime(&now) << "] " << pair.first << ": " << pair.second << "\n";
        }
        cout << "\n";
    }

    cout << "Goodbye!" << endl;
    return 0;
}

