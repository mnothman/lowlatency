#include <iostream>
#include <atomic>
#include <vector>
#include <string>
#include <thread>
#include <chrono>
#include <random>
#include <tbb/concurrent_hash_map.h> // Intel TBB for lock-free hash map
#include <tbb/concurrent_queue.h>   // For batch updates
#include <tbb/global_control.h>

// Data structure for stock prices
struct StockData {
    std::atomic<double> price;

    StockData() : price(0.0) {}
    StockData(double initialPrice) : price(initialPrice) {}

    // Custom move assignment operator
    StockData& operator=(StockData&& other) noexcept {
        price.store(other.price.load(std::memory_order_relaxed), std::memory_order_relaxed);
        return *this;
    }

    // Delete copy assignment (to make it explicit)
    StockData& operator=(const StockData&) = delete;
};

// Lock-free hash map for stock data
tbb::concurrent_hash_map<std::string, StockData> stockPrices;

// Random number generator for stock prices
double generateRandomPrice(double base, double range) {
    static std::mt19937 rng(std::random_device{}());
    std::uniform_real_distribution<double> dist(base - range, base + range);
    return dist(rng);
}

// Function to simulate batch updates
void simulateBatchUpdates() {
    std::vector<std::string> stocks = {"AAPL", "GOOGL", "AMZN", "MSFT", "TSLA"};
    while (true) {
        auto start_time = std::chrono::high_resolution_clock::now(); // Start timer

        tbb::concurrent_queue<std::pair<std::string, double>> updateQueue;

        // Generate prices for all stocks in batch
        for (const auto &stock : stocks) {
            double newPrice = generateRandomPrice(100.0, 50.0); // Generate random price
            updateQueue.push({stock, newPrice});
        }

        // Apply batch updates
        std::pair<std::string, double> update;
        while (updateQueue.try_pop(update)) {
            tbb::concurrent_hash_map<std::string, StockData>::accessor accessor;
            if (stockPrices.find(accessor, update.first)) {
                accessor->second.price.store(update.second, std::memory_order_relaxed);
            }
        }

        auto end_time = std::chrono::high_resolution_clock::now(); // End timer
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time).count();
        std::cout << "Batch update latency: " << duration << " microseconds" << std::endl;

        std::this_thread::sleep_for(std::chrono::milliseconds(50)); // Simulate latency
    }
}

// Function to query stock prices
void queryStockPrice(const std::string &stock) {
    tbb::concurrent_hash_map<std::string, StockData>::const_accessor accessor;
    while (true) {
        auto start_time = std::chrono::high_resolution_clock::now(); // Start timer

        if (stockPrices.find(accessor, stock)) {
            std::cout << "Stock: " << stock
                      << " Price: $" << accessor->second.price.load(std::memory_order_relaxed)
                      << std::endl;
        } else {
            std::cout << "Stock not found: " << stock << std::endl;
        }
        auto end_time = std::chrono::high_resolution_clock::now(); // End timer
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time).count();
        std::cout << "Query latency for " << stock << ": " << duration << " microseconds" << std::endl;

        std::this_thread::sleep_for(std::chrono::seconds(1)); // Query every second
; // Query every second
    }
}

int main() {
    // Initialize thread pool
    tbb::global_control globalLimit(tbb::global_control::max_allowed_parallelism, std::thread::hardware_concurrency());

    // Initialize stock data
    {
        tbb::concurrent_hash_map<std::string, StockData>::accessor accessor;
        stockPrices.insert(accessor, "AAPL");
        accessor->second = StockData(150.0);

        stockPrices.insert(accessor, "GOOGL");
        accessor->second = StockData(2800.0);

        stockPrices.insert(accessor, "AMZN");
        accessor->second = StockData(3400.0);

        stockPrices.insert(accessor, "MSFT");
        accessor->second = StockData(299.0);

        stockPrices.insert(accessor, "TSLA");
        accessor->second = StockData(720.0);
    }

    // Start the stock price update thread
    std::thread updateThread(simulateBatchUpdates);

    // Start query threads for different stocks using the thread pool
    std::thread queryThread1(queryStockPrice, "AAPL");
    std::thread queryThread2(queryStockPrice, "GOOGL");
    std::thread queryThread3(queryStockPrice, "MSFT");

    // Join threads (will not terminate in this demo)
    updateThread.join();
    queryThread1.join();
    queryThread2.join();
    queryThread3.join();

    return 0;
}
