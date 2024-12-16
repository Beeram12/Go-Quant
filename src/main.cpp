#include <iostream>
#include <string>
#include <unordered_set>
#include <curl/curl.h>
#include <nlohmann/json.hpp>
#include <websocketpp/config/asio_client.hpp>
#include <websocketpp/client.hpp>
#include <websocketpp/common/thread.hpp>
#include <thread>
#include <chrono>

typedef websocketpp::client<websocketpp::config::asio_tls_client> client;
using json = nlohmann::json;

//  Used by cURL to write the response from the server into a string.
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
    const std::string wsUrl = "wss://test.deribit.com/ws/api/v2/";
    client wsClient;
    websocketpp::connection_hdl hdl;
    std::thread wsThread;
    bool isConnected = false;
    std::unordered_set<std::string> subscribed_instruments;
    static int update_counter;

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
    // Constructor
    TradingClient(const std::string &id, const std::string &secretId) : clientId(id), clientSecretId(secretId)
    {
        wsClient.clear_access_channels(websocketpp::log::alevel::all);
        wsClient.clear_error_channels(websocketpp::log::elevel::all);
        wsClient.init_asio();
        wsClient.set_open_handler(std::bind(&TradingClient::on_open, this, std::placeholders::_1));
        wsClient.set_message_handler(std::bind(&TradingClient::on_message, this, std::placeholders::_1, std::placeholders::_2));
        wsClient.set_close_handler(std::bind(&TradingClient::on_close, this, std::placeholders::_1));
    }
    // Destructor
    ~TradingClient()
    {
        if (wsThread.joinable())
        {
            wsClient.stop();
            wsThread.join();
        }
    }

    void on_open(websocketpp::connection_hdl hdl)
    {
        this->hdl = hdl;
        isConnected = true;
        std::cout << "WebSocket connection established." << std::endl;
    }
    // Output From websocket
    void on_message(websocketpp::connection_hdl hdl, client::message_ptr msg)
    {
        std::string received_msg = msg->get_payload();
        try
        {
            json response = json::parse(received_msg);
            if (response.contains("params"))
            {
                update_counter++;
                std::cout << "Update #" << update_counter << std::endl;
                auto params = response["params"];
                if (params.contains("data"))
                {
                    auto data = params["data"];
                    static json previous_data;
                    if (data != previous_data)
                    {
                        std::cout << "Data updated: " << data.dump(4) << std::endl;
                        previous_data = data;
                    }
                    else
                    {
                        std::cout << "No new updates." << std::endl;
                    }
                }
                else if (params.contains("error"))
                {
                    std::cout << "Error: " << params["error"].dump(4) << std::endl;
                }
            }
        }
        catch (const std::exception &e)
        {
            std::cerr << "Error parsing WebSocket message: " << e.what() << std::endl;
        }
    }

    void on_close(websocketpp::connection_hdl hdl)
    {
        isConnected = false;
        std::cout << "WebSocket connection closed." << std::endl;
    }
    // Function to connect websocket
    void connectWebSocket()
    {
        websocketpp::lib::error_code ec;
        wsClient.set_tls_init_handler([](websocketpp::connection_hdl hdl) -> websocketpp::lib::shared_ptr<boost::asio::ssl::context>
                                      {
    websocketpp::lib::shared_ptr<boost::asio::ssl::context> ctx = 
        websocketpp::lib::make_shared<boost::asio::ssl::context>(boost::asio::ssl::context::tlsv12_client);
    try {
        ctx->set_verify_mode(boost::asio::ssl::context::verify_none);
        // Load certificates if needed
        // ctx->load_verify_file("path_to_certificate.pem");
    } catch (const std::exception &e) {
        std::cerr << "Error initializing SSL context: " << e.what() << std::endl;
    }
    return ctx; });

        client::connection_ptr con = wsClient.get_connection(wsUrl, ec);
        if (ec)
        {
            std::cerr << "WebSocket connection error: " << ec.message() << std::endl;
            return;
        }

        wsClient.connect(con);
        wsThread = std::thread([&]()
                               { wsClient.run(); });
    }

    // Function to send message through websocket
    void sendWebSocketMessage(const std::string &message)
    {
        if (isConnected)
        {
            // Start timer
            auto start_time = std::chrono::high_resolution_clock::now();
            wsClient.send(hdl, message, websocketpp::frame::opcode::text);
        }
        else
        {
            std::cerr << "Cannot send message. WebSocket not connected." << std::endl;
        }
    }
    // Function for Subscribing to orderBook
    void subscribeToOrderBook(const std::string &instrument, int duration_seconds)
    {
        std::cout << "Subscribed to:" << instrument << std::endl;
        subscribed_instruments.insert(instrument);
        json payload = {
            {"jsonrpc", "2.0"},
            {"method", "public/subscribe"},
            {"params", {{"channels", {"book." + instrument + ".100ms"}}}},
            {"id", 1}};
        sendWebSocketMessage(payload.dump());
        std::thread([this, duration_seconds]
                    {
            std::this_thread::sleep_for(std::chrono::seconds(duration_seconds));
            std::cout << "closing WebSocket connection after " << duration_seconds << " seconds." << std::endl;
            wsClient.close(hdl, 1000, "Closing after timeout"); })
            .detach();
    }
    // Function to show subscription
    void showSubscriptions()
    {
        std::cout << "Subscribed to:" << std::endl;
        for (const auto &instrument : subscribed_instruments)
        {
            std::cout << instrument << std::endl;
        }
    }
    // Function to authenticate and get accesstoken
    void authenticate()
    {
        json payload = {
            {"id", 0},
            {"method", "public/auth"},
            {"params", {{"grant_type", "client_credentials"}, {"client_id", clientId}, {"client_secret", clientSecretId}}},
            {"jsonrpc", "2.0"}};

        std::string res = sendRequest("public/auth", payload);
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
        if (!response.empty())
        {
            try
            {
                auto responseJson = json::parse(response);
                if (responseJson.contains("error") && responseJson["error"].contains("data") && responseJson["error"]["data"].contains("reason"))
                {
                    std::cerr << "Error Details: " << responseJson["error"]["data"]["reason"] << std::endl;
                }
                else if (responseJson.contains("message"))
                {
                    std::cerr << "Error Details: " << responseJson["message"] << std::endl;
                }
                else
                {
                    std::cout << "Order placed successfully." << std::endl;
                }
            }
            catch (const std::exception &e)
            {
                std::cerr << "Error parsing JSON response: " << e.what() << std::endl;
            }
        }
        else
        {
            std::cout << "No response received or error occurred." << std::endl;
        }
    }
    // Function to get all orders
    void getAllOpenOrders()
    {
        json payload = {
            {"jsonrpc", "2.0"},
            {"method", "private/get_open_orders"},
            {"params", {}},
            {"id", 2}};

        std::string res = sendRequest("private/get_open_orders", payload, accessToken);
        try
        {
            auto responseJson = json::parse(res);

            if (responseJson.contains("result"))
            {
                if (responseJson["result"].empty())
                {
                    std::cout << "No open orders found." << std::endl;
                }
                else
                {
                    auto orders = responseJson["result"];
                    std::cout << "All Open Orders in detail: " << orders.dump(4) << std::endl;
                    // Loop through orders safely
                    for (const auto &order : orders)
                    {

                        if (order.contains("order_id"))
                            std::cout << "Order ID: " << order["order_id"] << std::endl;

                        if (order.contains("instrument_name"))
                            std::cout << "Instrument: " << order["instrument_name"] << std::endl;

                        if (order.contains("price"))
                            std::cout << "Price: " << order["price"] << std::endl;

                        if (order.contains("quantity"))
                            std::cout << "Quantity: " << order["quantity"] << std::endl;
                        else
                            std::cout << "Quantity: N/A\n"
                                      << std::endl;
                    }
                }
            }
            else
            {
                std::cerr << "Failed to retrieve open orders: " << responseJson.dump(4) << std::endl;
            }
        }
        catch (const std::exception &e)
        {
            std::cerr << "An error occurred: " << e.what() << std::endl;
        }
    }
    // Function to cancel order
    void cancelOrder(const std::string &accesstoken, const std::string &orderId)
    {
        json payload = {
            {"jsonrpc", "2.0"},
            {"method", "private/cancel"},
            {"params", {{"order_id", orderId}}},
            {"id", 3}};
        std::string response = sendRequest("private/cancel", payload, accessToken);
        auto responseJson = json::parse(response);
        if (responseJson.contains("error"))
        {
            std::cerr << "Error cancelling order: " << responseJson["error"]["message"] << std::endl;
        }
        else
        {
            std::cout << "Cancelled Order: " << std::endl;
        }
    }
    // Function to modify order
    void modifyOrder(const std::string &accesstoken, const std::string &orderId, double newPrice, double newAmount)
    {
        json payload = {
            {"jsonrpc", "2.0"},
            {"method", "private/modifyOrder"},
            {"params", {{"order_id", orderId}, {"price", newPrice}, {"amount", newAmount}}},
            {"id", 4}};
        std::string response = sendRequest("private/edit", payload, accessToken);
        if (!response.empty())
        {
            try
            {
                auto responseJson = json::parse(response);
                if (responseJson.contains("error") && responseJson["error"].contains("data") && responseJson["error"]["data"].contains("reason"))
                {
                    std::cerr << "Error Details: " << responseJson["error"]["data"]["reason"] << std::endl;
                }
                else if (responseJson.contains("message"))
                {
                    std::cerr << "Error Details: " << responseJson["message"] << std::endl;
                }
                else
                {
                    std::cout << "Order modified successfully." << std::endl;
                }
            }
            catch (const std::exception &e)
            {
                std::cerr << "Error parsing JSON response: " << e.what() << std::endl;
            }
        }
        else
        {
            std::cout << "No response received or error occurred." << std::endl;
        }
    }
    // Function to get orderbook
    void getOrderBook(const std::string &instrument, int depth)
    {
        json payload = {
            {"jsonrpc", "2.0"},
            {"method", "public/get_order_book"},
            {"params", {{"instrument_name", instrument}, {"depth", depth}}},
            {"id", 5}};

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
    // Function to get position
    void getPositions(const std::string &accessToken, const std::string &currency, const std::string &kind)
    {
        json payload = {
            {"jsonrpc", "2.0"},
            {"method", "private/get_positions"},
            {"params", {
                           {"currency", currency},
                           {"kind", kind},
                       }},
            {"id", 6}};

        std::string response = sendRequest("private/get_positions", payload, accessToken);

        if (response.empty())
        {
            std::cout << "Currency is unavailable.\n";
            return;
        }
        else
        {
            try
            {
                auto responseJson = json::parse(response);

                if (responseJson.contains("result"))
                {
                    auto positions = responseJson["result"];

                    for (const auto &position : positions)
                    {
                        // Extract and print only the required fields with correct keys and default values
                        std::cout << "Instrument: " << position.value("instrument_name", "Unknown") << "\n";
                        std::cout << "Kind: " << position.value("kind", "Unknown") << "\n";
                        std::cout << "Size: " << position.value("size", 0) << "\n";
                        std::cout << "Average Price: " << position.value("average_price", 0.0) << "\n";
                        std::cout << "Direction: " << position.value("direction", "Unknown") << "\n";
                        std::cout << "Size BTC: " << position.value("size_currency", 0.0) << "\n";
                        std::cout << "Floating PnL: " << position.value("floating_profit_loss", 0.0) << "\n";
                        std::cout << "Realized PnL: " << position.value("realized_profit_loss", 0.0) << "\n";
                        std::cout << "Estimated Liquidation Price: "
                                  << (position.contains("estimated_liquidation_price") && !position["estimated_liquidation_price"].is_null()
                                          ? position["estimated_liquidation_price"].get<double>()
                                          : 0.0)
                                  << "\n";
                        std::cout << "Mark Price: " << position.value("mark_price", 0.0) << "\n";
                        std::cout << "Index Price: " << position.value("index_price", 0.0) << "\n";
                        std::cout << "Maintenance Margin: " << position.value("maintenance_margin", 0.0) << "\n";
                        std::cout << "Initial Margin: " << position.value("initial_margin", 0.0) << "\n";
                        std::cout << "Settlement Price: " << position.value("settlement_price", 0.0) << "\n";
                        std::cout << "Delta: " << position.value("delta", 0.0) << "\n";
                        std::cout << "Open Order Margin: " << position.value("open_orders_margin", 0.0) << "\n";
                        std::cout << "Profit/Loss: " << position.value("total_profit_loss", 0.0) << "\n";
                        std::cout << "-----------------------------------------\n";
                    }
                }
                else if (responseJson.contains("error") && responseJson["error"].contains("data") && responseJson["error"]["data"].contains("reason"))
                {
                    std::cerr << "Error Details: " << responseJson["error"]["data"]["reason"] << std::endl;
                }
                else if (responseJson.contains("message"))
                {
                    std::cerr << "Error Details: " << responseJson["message"] << std::endl;
                }
                else
                {
                    std::cerr << "Unexpected response format: " << response << "\n";
                }
            }
            catch (const std::exception &e)
            {
                std::cerr << "Failed to parse response: " << e.what() << "\n";
                std::cerr << "Raw response: " << response << "\n";
            }
        }
    }
};
int TradingClient::update_counter = 0;
int main()
{

    std::string clientId, clientSecret;
    std::cout << "Enter your publicId: ";
    std::cin >> clientId;
    std::cout << "Enter your privateId: ";
    std::cin >> clientSecret;

    // Creating client object
    TradingClient client(clientId, clientSecret);

    // Authenticating
    client.authenticate();

    // Getting the access token
    const std::string accesstoken = client.getAccessToken();
    if (!client.getAccessToken().empty())
    {
        while (true)
        {
            int choice;
            std::cout << "\nSelect an option:\n";
            std::cout << "1. Place Order\n";
            std::cout << "2. Get All Open Orders\n";
            std::cout << "3. Get Order Book\n";
            std::cout << "4. Modify Order\n";
            std::cout << "5. Cancel Order\n";
            std::cout << "6. Get Positions\n";
            std::cout << "7.Subscribe to Orderbook\n";
            std::cout << "8.Show all subscriptions\n";
            std::cout << "9. Exit\n";
            std::cout << "Enter your choice: ";
            std::cin >> choice;

            switch (choice)
            {
            case 1:
            {
                // Place Order
                std::string instrument;
                double price, amount;
                std::cout << "Enter instrument name: ";
                std::cin >> instrument;
                std::cout << "Enter price: ";
                std::cin >> price;
                std::cout << "Enter amount: ";
                std::cin >> amount;
                client.placeOrder(instrument, accesstoken, price, amount);
                break;
            }
            case 2:
            {
                // Get All Open Orders
                client.getAllOpenOrders();
                break;
            }
            case 3:
            {
                // Get Order Book
                std::string instrument;
                int depth;
                std::cout << "Enter instrument name: ";
                std::cin >> instrument;
                std::cout << "Enter depth: ";
                std::cin >> depth;
                client.getOrderBook(instrument, depth);
                break;
            }
            case 4:
            {
                // Modify Order
                std::string instrument;
                int price, amount;
                std::cout << "Enter instrument name: ";
                std::cin >> instrument;
                std::cout << "Enter price: ";
                std::cin >> price;
                std::cout << "Enter amount: ";
                std::cin >> amount;
                client.modifyOrder(instrument, accesstoken, price, amount);
                break;
            }
            case 5:
            {
                // Cancel Order
                std::string orderId;
                std::cout << "Enter order id: ";
                std::cin >> orderId;
                client.cancelOrder(accesstoken, orderId);
                break;
            }
            case 6:
            {
                // Get Positions
                std::string currency;
                std::cout << "Enter currency like BTC,ETH,EURR,USDC: ";
                std::cin >> currency;
                std::string kind;
                std::cout << "Enter kind like future,option,spot,: ";
                std::cin >> kind;
                client.getPositions(accesstoken, currency, kind);
                break;
            }
            case 7:
            {
                // Subscribe to Orderbook
                std::string instrument;
                std::cout << "Enter instrument name: ";
                std::cin >> instrument;
                int duration;
                std::cout << "Enter duration to get message:";
                std::cin >> duration;
                client.connectWebSocket();
                std::this_thread::sleep_for(std::chrono::seconds(2)); // Give some time for the connection to establish
                // Subscribe to the order book for the instrument you want
                client.subscribeToOrderBook(instrument, duration); // You can pass the instrument here
                std::this_thread::sleep_for(std::chrono::seconds(30));
                break;
            }
            case 8:
            {
                // Get all subscriptions
                client.showSubscriptions();
                break;
            }
            case 9:
            {
                // Exit
                std::cout << "Exiting program...\n";
                return 0;
            }
            default:
                std::cout << "Invalid choice. Please try again.\n";
                break;
            }
        }
    }
    else
    {
        std::cerr << "Access token not retrieved. Exiting." << std::endl;
    }
    return 0;
}
