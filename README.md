# Trading Client Project

## Overview
This project implements a trading client for interacting with the Deribit API. It supports functionalities such as authentication, market data subscription, order placement, and retrieving open orders. The client uses cURL for HTTP request and WebSocket for real-time communication and data processing.


## Requirements
### System Requirements
- Operating System: Linux(Ubuntu)
- Compiler: GCC (recommended) or any C++ compiler with c++17 support.
- Build Tool: CMake 3.10 or later
- Network: Stable internet connection for API access.

### Dependencies
The following libraries and tools are required to build and run the project:

1. **[cURL](https://curl.se/download.html)**:
   - Used for making jsonRPC HTTP requests.
   - **Installation**:
     - Linux: 
```bash
sudo apt install libcurl4-openssl-dev
```
     

2. **[nlohmann/json](https://github.com/nlohmann/json)**:
   - A C++ JSON library for parsing API responses.
   - **Installation**:
       - Linux: 
```bash
sudo apt install nlohmann-json3-dev
```
       

3. **[websocketpp](https://github.com/zaphoyd/websocketpp)**:
   - A C++ WebSocket library for real-time communication.
   - **Installation**:
     - Linux(Ubuntu):
```bash
sudo apt update
sudo apt install libwebsocketpp-dev
```
   - ***Verify Installation***:
        - Check that the WebSocket++ header files are installed at 
            `/usr/include/websocketpp`

4. **[Boost](https://www.boost.org/)**:
   - Required by `websocketpp` for ASIO and SSL.
   - **Installation**:
     - Linux: 
```bash
sudo apt install libboost-all-dev
```

5. **CMake**:
   - Build system generator.
   - **Installation**:
     - Linux: 
```bash
sudo apt install cmake
```

6. **[OpenSSL](https://www.openssl.org/)**:
   - Provides TLS/SSL support for secure WebSocket connections.
   - **Installation**:
     - Linux: 
```bash
sudo apt install libssl-dev
```


## Installation Instructions

### Step 1: Clone the Repository
```bash
git clone https://github.com/Beeram12/TradingManagement.git
cd TradingManagement
```
### Step 2: Install Dependencies
   - Make sure all dependencies listed above are installed

### Step 3: Build the Project
Use CMake to configure and build the project:
```bash
mkdir build
cd build
cmake ..
make
```

## Usage 

1. **Running the Client**
```bash
./TradingClient
```
## Features

- **Authenticate**: Logs in using your client ID and secret.
- **Subscribe to Market Data**: Receives live updates on specified instruments.
- **Place Orders**: Sends orders to the exchange.
- **Cancel Orders**: Cancels the orders accordingly.
- **Modifies Orders**:Modifies the order details as per requirement
- **Get OrderBook**:Able to retrieve orderbook for required instrument
- **View Positions**:Able to view positions of placed order

## Directory Structure
```bash
.
├── build/                  # Build directory (ignored in .gitignore)
├── CMakeLists.txt          # CMake configuration
├── README.md               # Project documentation
├── src/
│   └── main.cpp            # Main source file
```

## **Benchmarking Results**

This section summarizes the performance metrics collected during testing for different aspects of the system, including **order placement latency**, **market data processing latency**, **WebSocket message propagation delay**, and **end-to-end trading loop latency**.



### **1. Order Placement Latency**
Measures the time taken to send an order to the server and receive a response.

| **Instrument Name**      | **Price** | **Amount** | **Time (ms)** |
|---------------------------|-----------|------------|---------------|
| ETH-PERPETUAL            | 20        | 20         | 911.488       |
| ADA_USDC-PERPETUAL       | 1.04      | 0.2        | 1178.85       |
| BTC_EURR                 | 20        | 20         | 992.463       |





### **2. Market Data Processing Latency**
Time taken from receiving market data updates via WebSocket to processing and updating the internal state.

| **Instrument Name**      | **Time (µs)** |
|---------------------------|---------------|
| ETH-PERPETUAL            | 360           |
| BTC-PERPETUAL            | 591           |
| DOGE_USDC-PERPETUAL      | 573           |


### **3. WebSocket Message Propagation Delay**
Measures the time taken for a WebSocket message to travel from the server to the client.

| **Instrument Name**      | **Time (µs)** |
|---------------------------|---------------|
| ETH-PERPETUAL            | 0             |
| BTC-PERPETUAL            | 0             |
| DOGE_USDC-PERPETUAL      | 0             |


### **4. End-to-End Trading Loop Latency**
Measures the time taken for a complete cycle: from placing an order to receiving a server response.

| **Instrument Name**      | **Price** | **Amount** | **Time (ms)** |
|---------------------------|-----------|------------|---------------|
| ETH-PERPETUAL            | 20        | 20         | 847           |
| ADA_USDC-PERPETUAL       | 1.04      | 0.2        | 1216          |
| BTC_EURR                 | 20        | 20         | 873           |



## **Key Takeaways**
1. **Order Placement Latency**: Average latency for placing orders was approximately 1027.60 ms.
2. **Market Data Processing Latency**: Market data updates were processed in an average of ~508 µs.
3. **WebSocket Propagation Delay**: Negligible due to local testing.
4. **End-to-End Trading Loop Latency**: Average cycle latency was measured at 978.66 ms.




