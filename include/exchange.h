#pragma once

#include <string>
#include <vector>

/**
 * Exchange Types
 */
enum class ExchangeType {
    EQUITY,      // Stock exchanges (NYSE, NASDAQ, LSE)
    DERIVATIVES, // Futures/options (CME, EUREX)
    CRYPTO,      // Cryptocurrency (Binance, Coinbase)
    FOREX        // Foreign exchange
};

/**
 * Represents a single exchange with geographic location
 */
struct Exchange {
    std::string id;          // Short identifier (e.g., "NYSE")
    std::string name;        // Full name
    std::string city;        // City location
    double latitude;         // GPS latitude (-90 to 90)
    double longitude;        // GPS longitude (-180 to 180)
    ExchangeType type;       // Exchange type
    
    // Trading parameters
    double fee_percent = 0.1;      // Trading fee (default 0.1%)
    double min_profit_bps = 5.0;   // Minimum profit in basis points
    bool is_active = true;         // Is exchange operational?
    
    // Constructor
    Exchange(const std::string& id, const std::string& name, 
             const std::string& city, double lat, double lon, ExchangeType type)
        : id(id), name(name), city(city), 
          latitude(lat), longitude(lon), type(type) {}
    
    // Default constructor for STL containers
    Exchange() : latitude(0), longitude(0), type(ExchangeType::EQUITY) {}
    
    // Get exchange type as string
    std::string get_type_string() const {
        switch (type) {
            case ExchangeType::EQUITY: return "Equity";
            case ExchangeType::DERIVATIVES: return "Derivatives";
            case ExchangeType::CRYPTO: return "Crypto";
            case ExchangeType::FOREX: return "Forex";
            default: return "Unknown";
        }
    }
};

/**
 * Convert string to ExchangeType
 */
inline ExchangeType string_to_exchange_type(const std::string& str) {
    if (str == "equity") return ExchangeType::EQUITY;
    if (str == "derivatives") return ExchangeType::DERIVATIVES;
    if (str == "crypto") return ExchangeType::CRYPTO;
    if (str == "forex") return ExchangeType::FOREX;
    return ExchangeType::EQUITY; // Default
}