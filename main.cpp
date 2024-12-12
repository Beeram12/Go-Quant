#include <iostream>
#include <string>
#include <curl/curl.h>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

// utility function for cURL response handling
size_t WriteCallback(void *contents, size_t size, size_t nmemb, void *userp)
{
    ((std::string *)userp)->append((char *)contents, size * nmemb);
    return size * nmemb;
}

class TradingClient
{
private:
    std::string clientId;
    std::string clientSecretId;
    std::string accessToken;
    const std::string baseUrl = "https://test.deribit.com/api/v2/";

    // Function to send a cURL request
    std::string sendRequest(const std::string &endpoint, const json &payload, const std::string &token = "")
    {
        std::string readBuffer;
        CURL *curl;
        CURLcode res;

        curl_global_init(CURL_GLOBAL_DEFAULT);
        curl = curl_easy_init();

        if (curl)
        {
            std::string url = baseUrl + endpoint; // Combine base URL and endpoint
            curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
            curl_easy_setopt(curl, CURLOPT_POST, 1L);

            std::string jsonStr = payload.dump();
            curl_easy_setopt(curl, CURLOPT_POSTFIELDS, jsonStr.c_str());

            struct curl_slist *headers = NULL;
            headers = curl_slist_append(headers, "Content-Type: application/json");
            if (!token.empty())
            {
                headers = curl_slist_append(headers, ("Authorization: Bearer " + token).c_str());
            }
            curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
            curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
            curl_easy_setopt(curl, CURLOPT_WRITEDATA, &readBuffer);

            res = curl_easy_perform(curl);
            if (res != CURLE_OK)
            {
                std::cerr << "cURL Error: " << curl_easy_strerror(res) << std::endl;
            }

            curl_slist_free_all(headers);
            curl_easy_cleanup(curl);
        }

        curl_global_cleanup();
        return readBuffer;
    }

public:
    const std::string &getAccessToken() const
    {
        return accessToken;
    }

    TradingClient(const std::string &id, const std::string &secretId)
    {
        clientId = id;
        clientSecretId = secretId;
    }

    void authenticate()
    {
        json payload = {
            {"id", 0},
            {"method", "public/auth"},
            {"params", {{"grant_type", "client_credentials"}, {"client_id", clientId}, {"client_secret", clientSecretId}}},
            {"jsonrpc", "2.0"}};

        std::string res = sendRequest("public/auth", payload);
        // std::cout << "Raw Response: " << res << std::endl; // Debug line
        auto responseJson = json::parse(res);

        if (responseJson.contains("result") && responseJson["result"].contains("access_token"))
        {
            accessToken = responseJson["result"]["access_token"];
            std::cout << "Access token retrieved successfully." << std::endl;
        }
        else
        {
            std::cerr << "Failed to authenticate." << std::endl;
            if (responseJson.contains("error"))
            {
                std::cerr << "Error Details: " << responseJson["error"].dump() << std::endl;
            }
        }
    }

    // For placing order
    void placeOrder(const std::string &instrument, const std::string &accessToken, double price, double amount)
    {
        json payload = {
            {"jsonrpc", "2.0"},
            {"method", "private/buy"},
            {"params", {
                           {"instrument_name", instrument},
                           {"type", "limit"},
                           {"price", price},
                           {"amount", amount},
                       }},
            {"id", 1}};
        std::string response = sendRequest("private/buy", payload, accessToken);
        std::cout << "Placed order: " << response << std::endl;
    }

    void cancelOrder(const std::string &accesstoken, const std::string &orderId)
    {
        json payload = {
            {"jsonrpc", "2.0"},
            {"method", "private/cancelOrder"},
            {"params", {{"order_id", orderId}}},
            {"id", 1}};
        std::string response = sendRequest("private/cancel", payload, accessToken);
        std::cout << "Cancelled Order: " << response << std::endl;
    }

    void modifyOrder(const std::string &accesstoken, const std::string &orderId, double newPrice, double newAmount)
    {
        json payload = {
            {"jsonrpc", "2.0"},
            {"method", "private/modifyOrder"},
            {"params", {{"order_id", orderId}, {"price", newPrice}, {"amount", newAmount}}},
            {"id", 1}};
        std::string response = sendRequest("private/edit", payload, accessToken);
        std::cout << "Modify Order Response: " << response << std::endl;
    }

    void getOrderBook(const std::string &instrument, int depth)
    {
        json payload = {
            {"jsonrpc", "2.0"},
            {"method", "public/get_order_book"},
            {"params", {{"instrument_name", instrument}, {"depth", depth}}},
            {"id", 4}};

        std::string response = sendRequest("public/get_order_book", payload);
        auto responseJson = json::parse(response);

        if (responseJson.contains("result"))
        {
            const auto &result = responseJson["result"];

            std::cout << "Order Book for " << instrument << ":\n";

            // Print general details
            std::cout << "Timestamp: " << result["timestamp"] << '\n';
            std::cout << "State: " << result["state"] << '\n';
            std::cout << "Index Price: " << result["index_price"] << '\n';
            std::cout << "Last Price: " << result["last_price"] << '\n';
            std::cout << "Settlement Price: " << result["settlement_price"] << '\n';
            std::cout << "Min Price: " << result["min_price"] << '\n';
            std::cout << "Max Price: " << result["max_price"] << '\n';
            std::cout << "Open Interest: " << result["open_interest"] << '\n';
            std::cout << "Mark Price: " << result["mark_price"] << '\n';
            std::cout << "Best Bid Price: " << result["best_bid_price"]
                      << ", Amount: " << result["best_bid_amount"] << '\n';
            std::cout << "Best Ask Price: " << result["best_ask_price"]
                      << ", Amount: " << result["best_ask_amount"] << '\n';

            // Print bids
            if (result.contains("bids"))
            {
                std::cout << "\nBids:\n";
                for (const auto &bid : result["bids"])
                {
                    std::cout << "Price: " << bid[0] << ", Amount: " << bid[1] << '\n';
                }
            }

            // Print asks
            if (result.contains("asks"))
            {
                std::cout << "\nAsks:\n";
                for (const auto &ask : result["asks"])
                {
                    std::cout << "Price: " << ask[0] << ", Amount: " << ask[1] << '\n';
                }
            }
        }
        else
        {
            std::cerr << "Failed to retrieve order book." << std::endl;
            if (responseJson.contains("error"))
            {
                std::cerr << "Error Details: " << responseJson["error"].dump() << std::endl;
            }
        }
    }

    void getPositions(const std::string &accesstoken, const std::string &currency, const std::string &kind)
    {
        json payload = {
            {"jsonrpc", "2.0"},
            {"method", "private/get_positions"},
            {"params", {
                           {"currency", currency},
                           {"kind", kind},
                       }},
            {"id", 5}};

        std::string response = sendRequest("private/get_positions", payload, accessToken);
        auto responseJson = json::parse(response);
        if (responseJson.contains("result"))
        {
            auto positions = responseJson["result"];
            for (const auto &position : positions)
            {
                // Extract and print position details
                std::cout << "Instrument: " << position["instrument_name"] << "\n";
                std::cout << "Kind: " << position["kind"] << "\n";
                std::cout << "Direction: " << position["direction"] << "\n";
                std::cout << "Size: " << position["size"] << "\n";
                std::cout << "Delta: " << position["delta"] << "\n";
                std::cout << "Gamma: " << position.value("gamma", 0.0) << "\n"; // Optional for futures
                std::cout << "Theta: " << position.value("theta", 0.0) << "\n"; // Optional for futures
                std::cout << "Vega: " << position.value("vega", 0.0) << "\n";   // Optional for futures
                std::cout << "Floating Profit/Loss: " << position["floating_profit_loss"] << "\n";
                std::cout << "Mark Price: " << position["mark_price"] << "\n";
                std::cout << "Realized Profit/Loss: " << position["realized_profit_loss"] << "\n";
                std::cout << "Total Profit/Loss: " << position["total_profit_loss"] << "\n";
                std::cout << "-----------------------------------------\n";
            }
        }
        else if (responseJson.contains("error"))
        {
            auto error = responseJson["error"];
            std::cerr << "Error (" << error["code"] << "): " << error["message"] << "\n";
        }
        else
        {
            std::cerr << "Unexpected response format: " << response << "\n";
        }
    }
};

int main()
{
    std::string clientId;
    std::string clientPrivateId;
    std::cout << "Enter your public key:";
    std::cin >> clientId;
    std::cout << "Enter your private Key:";
    std::cin >> clientPrivateId;
    // Creating Object;
    TradingClient client(clientId, clientPrivateId);

    client.authenticate();

    if (!client.getAccessToken().empty())
    {
        const std::string accesstoken = client.getAccessToken();
        // client.placeOrder("BTC-PERPETUAL",accesstoken, 228, 20000);
        // client.placeOrder("SOL_USDC-PERPETUAL",accesstoken, 2,2001);
        // client.getOrderBook("BCH_USDC-PERPETUAL", 1000);
        client.getPositions(accesstoken,"BTC","spot");
        
    }
    else
    {
        std::cerr << "AccessToken not received, Exiting" << std::endl;
    }

    return 0;
}
