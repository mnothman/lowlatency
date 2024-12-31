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

// Use atomic, thread safe
struct StockData {
    std::atomic<double> price;

    StockData() : price(0.0) {}
    StockData(double initialPrice) : price(initialPrice) {}

    // Maintain atomic thread safety for move assignment   
    StockData& operator=(StockData&& other) noexcept {
        price.store(other.price.load(std::memory_order_relaxed), std::memory_order_relaxed);
        return *this;
    }

    // Delete so we don't accidentally copy atomic variables
    StockData& operator=(const StockData&) = delete;
};

// Use lock free hash map for data
// Use Intel TBB concurrent_hash_map to reduce contention and avoid traditional mutex based locking
tbb::concurrent_hash_map<std::string, StockData> stockPrices;

// Random number generator for stock prices
double generateRandomPrice(double base, double range) {
    static std::mt19937 rng(std::random_device{}());
    std::uniform_real_distribution<double> dist(base - range, base + range);
    return dist(rng);
}

// Do batch updates in a single operation for efficiency and to reduce contention
void simulateBatchUpdates() {
    std::vector<std::string> stocks = {"AAPL", "GOOGL", "AMZN", "MSFT", "TSLA"};
    while (true) {
        auto start_time = std::chrono::high_resolution_clock::now(); // Start timer

        tbb::concurrent_queue<std::pair<std::string, double>> updateQueue;

        for (const auto &stock : stocks) {
            double newPrice = generateRandomPrice(100.0, 50.0); // Generate random price
            updateQueue.push({stock, newPrice});
        }

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

// Use lock free accessor for low latency and high throughput
void queryStockPrice(const std::string &stock) {
    tbb::concurrent_hash_map<std::string, StockData>::const_accessor accessor;
    while (true) {
        auto start_time = std::chrono::high_resolution_clock::now(); // Start timer
        // Lock free access/lookup
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
    }
}

int main() {
    // Limit maximum number of threads that can run in parallel to the number of hardware threads available
    tbb::global_control globalLimit(tbb::global_control::max_allowed_parallelism, std::thread::hardware_concurrency());

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

    std::thread updateThread(simulateBatchUpdates);

    std::thread queryThread1(queryStockPrice, "AAPL");
    std::thread queryThread2(queryStockPrice, "GOOGL");
    std::thread queryThread3(queryStockPrice, "MSFT");

    updateThread.join();
    queryThread1.join();
    queryThread2.join();
    queryThread3.join();

    return 0;
}
