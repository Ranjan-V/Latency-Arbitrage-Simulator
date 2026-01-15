#pragma once

#include <vector>
#include <string>
#include <algorithm>
#include "exchange.h"
#include "price_feed.h"
#include "latency_calculator.h"
#include "network_graph.h"

/**
 * Represents a single arbitrage opportunity
 */
struct ArbitrageOpportunity {
    std::string buy_exchange;      // Where to buy
    std::string sell_exchange;     // Where to sell
    double buy_price;              // Purchase price (ask)
    double sell_price;             // Sale price (bid)
    double price_diff;             // Absolute price difference
    double profit_percent;         // Profit percentage (before fees)
    double latency_ms;             // One-way network latency
    double rtt_ms;                 // Round-trip time
    double estimated_profit;       // Net profit after fees
    double opportunity_window_ms;  // How long opportunity lasts
    bool is_executable;            // Can we execute in time?
    uint64_t timestamp;            // When opportunity was detected
    
    // For ranking
    double score;                  // Overall opportunity score
    
    ArbitrageOpportunity() : 
        buy_price(0), sell_price(0), price_diff(0), profit_percent(0),
        latency_ms(0), rtt_ms(0), estimated_profit(0), opportunity_window_ms(0),
        is_executable(false), timestamp(0), score(0) {}
};

/**
 * Arbitrage Scanner - Detects and ranks trading opportunities
 */
class ArbitrageScanner {
private:
    const NetworkGraph& network;
    const PriceFeed& price_feed;
    
    // Configuration
    double min_profit_bps = 5.0;           // Minimum 5 basis points profit
    double trading_fee_percent = 0.1;      // 0.1% per trade
    double slippage_percent = 0.05;        // 0.05% slippage
    double avg_opportunity_window_ms = 200.0; // Average window duration
    TransmissionMedium medium = TransmissionMedium::FIBER_OPTIC;
    
public:
    ArbitrageScanner(const NetworkGraph& net, const PriceFeed& feed)
        : network(net), price_feed(feed) {}
    
    /**
     * Scan for all arbitrage opportunities
     */
    std::vector<ArbitrageOpportunity> scan_opportunities() {
        std::vector<ArbitrageOpportunity> opportunities;
        const auto& exchanges = network.get_exchanges();
        const auto& prices = price_feed.get_all_prices();
        
        // Compare every pair of exchanges
        for (size_t i = 0; i < exchanges.size(); i++) {
            for (size_t j = i + 1; j < exchanges.size(); j++) {
                const auto& ex1 = exchanges[i];
                const auto& ex2 = exchanges[j];
                
                // Get prices
                auto price1_it = prices.find(ex1.id);
                auto price2_it = prices.find(ex2.id);
                
                if (price1_it == prices.end() || price2_it == prices.end()) {
                    continue;
                }
                
                const auto& quote1 = price1_it->second;
                const auto& quote2 = price2_it->second;
                
                // Check both directions
                // Direction 1: Buy at ex1, sell at ex2
                auto opp1 = evaluate_opportunity(ex1, ex2, quote1, quote2);
                if (opp1.is_executable && opp1.estimated_profit > 0) {
                    opportunities.push_back(opp1);
                }
                
                // Direction 2: Buy at ex2, sell at ex1
                auto opp2 = evaluate_opportunity(ex2, ex1, quote2, quote1);
                if (opp2.is_executable && opp2.estimated_profit > 0) {
                    opportunities.push_back(opp2);
                }
            }
        }
        
        // Rank opportunities by score
        std::sort(opportunities.begin(), opportunities.end(),
                 [](const ArbitrageOpportunity& a, const ArbitrageOpportunity& b) {
                     return a.score > b.score;
                 });
        
        return opportunities;
    }
    
    /**
     * Evaluate a single arbitrage opportunity
     */
    ArbitrageOpportunity evaluate_opportunity(
        const Exchange& buy_ex, 
        const Exchange& sell_ex,
        const PriceQuote& buy_quote,
        const PriceQuote& sell_quote) {
        
        ArbitrageOpportunity opp;
        opp.buy_exchange = buy_ex.id;
        opp.sell_exchange = sell_ex.id;
        opp.buy_price = buy_quote.ask;  // We pay the ask price
        opp.sell_price = sell_quote.bid; // We receive the bid price
        opp.timestamp = buy_quote.timestamp;
        
        // Calculate price difference
        opp.price_diff = opp.sell_price - opp.buy_price;
        opp.profit_percent = (opp.price_diff / opp.buy_price) * 100.0;
        
        // Calculate network latency
        opp.latency_ms = network.shortest_path_latency(buy_ex.id, sell_ex.id);
        opp.rtt_ms = opp.latency_ms * 2.0;
        
        // Estimate opportunity window (how long price discrepancy lasts)
        opp.opportunity_window_ms = avg_opportunity_window_ms;
        
        // Check if executable (RTT must be less than window)
        opp.is_executable = (opp.rtt_ms < opp.opportunity_window_ms);
        
        // Calculate net profit after fees and slippage
        double gross_profit = opp.price_diff;
        double trading_fees = opp.buy_price * (trading_fee_percent / 100.0) * 2; // Buy + sell
        double slippage_cost = opp.buy_price * (slippage_percent / 100.0);
        
        opp.estimated_profit = gross_profit - trading_fees - slippage_cost;
        
        // Calculate opportunity score (multi-factor)
        // Higher profit % + Lower latency + Larger window = Better score
        double profit_factor = opp.profit_percent * 10.0;
        double latency_factor = std::max(0.0, 100.0 - opp.latency_ms);
        double window_factor = opp.opportunity_window_ms / 100.0;
        
        opp.score = profit_factor + latency_factor + window_factor;
        
        // Only consider opportunities above minimum profit threshold
        double min_profit_percent = min_profit_bps / 100.0;
        if (opp.profit_percent < min_profit_percent) {
            opp.is_executable = false;
            opp.score = 0;
        }
        
        return opp;
    }
    
    /**
     * Get top N opportunities
     */
    std::vector<ArbitrageOpportunity> get_top_opportunities(int n) {
        auto all_opps = scan_opportunities();
        if (all_opps.size() > static_cast<size_t>(n)) {
            all_opps.resize(n);
        }
        return all_opps;
    }
    
    /**
     * Configuration setters
     */
    void set_min_profit_bps(double bps) { min_profit_bps = bps; }
    void set_trading_fee(double fee) { trading_fee_percent = fee; }
    void set_slippage(double slip) { slippage_percent = slip; }
    void set_opportunity_window(double window_ms) { avg_opportunity_window_ms = window_ms; }
    void set_transmission_medium(TransmissionMedium med) { medium = med; }
    
    /**
     * Get statistics
     */
    struct ScannerStats {
        int total_opportunities;
        int executable_opportunities;
        double avg_profit_percent;
        double max_profit_percent;
        double avg_latency_ms;
    };
    
    ScannerStats get_statistics() {
        auto opps = scan_opportunities();
        ScannerStats stats{};
        
        stats.total_opportunities = opps.size();
        stats.executable_opportunities = 0;
        stats.avg_profit_percent = 0;
        stats.max_profit_percent = 0;
        stats.avg_latency_ms = 0;
        
        if (opps.empty()) return stats;
        
        for (const auto& opp : opps) {
            if (opp.is_executable) stats.executable_opportunities++;
            stats.avg_profit_percent += opp.profit_percent;
            stats.max_profit_percent = std::max(stats.max_profit_percent, opp.profit_percent);
            stats.avg_latency_ms += opp.latency_ms;
        }
        
        stats.avg_profit_percent /= opps.size();
        stats.avg_latency_ms /= opps.size();
        
        return stats;
    }
};