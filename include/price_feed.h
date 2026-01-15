#pragma once

#include <string>
#include <map>
#include <random>
#include <chrono>
#include "exchange.h"

/**
 * Represents a price quote at a specific time
 */
struct PriceQuote {
    std::string exchange_id;
    std::string symbol;
    double bid;           // Buy price
    double ask;           // Sell price
    double last;          // Last traded price
    double volume;        // Trading volume
    uint64_t timestamp;   // Milliseconds since epoch
    
    double spread() const { return ask - bid; }
    double mid_price() const { return (bid + ask) / 2.0; }
};

/**
 * Mock price feed generator
 * Simulates realistic price movements with random walk + noise
 */
class PriceFeed {
private:
    std::map<std::string, PriceQuote> current_prices;
    std::mt19937 rng;
    std::normal_distribution<double> price_change_dist;
    std::normal_distribution<double> spread_dist;
    
    double base_price = 50000.0;  // Base price (e.g., BTC in USD)
    double volatility = 0.0002;    // 0.02% per update
    double base_spread_bps = 2.0;  // 2 basis points spread
    
public:
    PriceFeed() : price_change_dist(0.0, 1.0), spread_dist(0.0, 0.3) {
        auto seed = std::chrono::high_resolution_clock::now().time_since_epoch().count();
        rng.seed(static_cast<unsigned int>(seed));
    }
    
    /**
     * Initialize price feeds for all exchanges
     */
    void initialize_feeds(const std::vector<Exchange>& exchanges, const std::string& symbol = "BTC/USD") {
        for (const auto& ex : exchanges) {
            PriceQuote quote;
            quote.exchange_id = ex.id;
            quote.symbol = symbol;
            quote.last = base_price;
            quote.volume = 1000.0 + (std::rand() % 9000);
            quote.timestamp = get_current_timestamp();
            
            // Add small random offset per exchange (geographic factors)
            double offset = (std::rand() % 100 - 50) * 0.1;
            quote.last += offset;
            
            // Calculate bid/ask from mid price
            double spread_bps = base_spread_bps + spread_dist(rng);
            double spread_amount = quote.last * (spread_bps / 10000.0);
            quote.bid = quote.last - spread_amount / 2.0;
            quote.ask = quote.last + spread_amount / 2.0;
            
            current_prices[ex.id] = quote;
        }
    }
    
    /**
     * Update all prices (random walk simulation)
     */
    void update_prices() {
        uint64_t now = get_current_timestamp();
        
        // Global market movement (affects all exchanges similarly)
        double global_change = price_change_dist(rng) * volatility * base_price;
        
        for (auto& [exchange_id, quote] : current_prices) {
            // Individual exchange noise
            double local_noise = price_change_dist(rng) * volatility * base_price * 0.3;
            
            // Update last price
            quote.last += global_change + local_noise;
            
            // Ensure price stays positive
            if (quote.last < 100.0) quote.last = 100.0;
            
            // Update bid/ask
            double spread_bps = base_spread_bps + std::abs(spread_dist(rng));
            double spread_amount = quote.last * (spread_bps / 10000.0);
            quote.bid = quote.last - spread_amount / 2.0;
            quote.ask = quote.last + spread_amount / 2.0;
            
            // Update timestamp
            quote.timestamp = now;
            
            // Random volume fluctuation
            quote.volume += (std::rand() % 200 - 100);
            if (quote.volume < 100) quote.volume = 100;
        }
    }
    
    /**
     * Inject artificial arbitrage opportunity
     * Makes one exchange's price deviate significantly
     */
    void inject_arbitrage_opportunity(const std::string& exchange_id, double deviation_percent) {
        if (current_prices.count(exchange_id)) {
            current_prices[exchange_id].last *= (1.0 + deviation_percent / 100.0);
            
            // Update bid/ask
            double spread_bps = base_spread_bps;
            double spread_amount = current_prices[exchange_id].last * (spread_bps / 10000.0);
            current_prices[exchange_id].bid = current_prices[exchange_id].last - spread_amount / 2.0;
            current_prices[exchange_id].ask = current_prices[exchange_id].last + spread_amount / 2.0;
        }
    }
    
    /**
     * Get current price for an exchange
     */
    const PriceQuote* get_price(const std::string& exchange_id) const {
        auto it = current_prices.find(exchange_id);
        return (it != current_prices.end()) ? &it->second : nullptr;
    }
    
    /**
     * Get all current prices
     */
    const std::map<std::string, PriceQuote>& get_all_prices() const {
        return current_prices;
    }
    
    /**
     * Set volatility (0.0 to 1.0)
     */
    void set_volatility(double vol) {
        volatility = vol;
    }
    
    /**
     * Set base spread in basis points
     */
    void set_base_spread(double spread_bps) {
        base_spread_bps = spread_bps;
    }
    
private:
    uint64_t get_current_timestamp() const {
        return std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()
        ).count();
    }
};