#pragma once

#define _USE_MATH_DEFINES
#include <cmath>
#include "exchange.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/**
 * Network transmission medium
 */
enum class TransmissionMedium {
    FIBER_OPTIC,  // ~200,000 km/s (67% speed of light)
    MICROWAVE,    // ~300,000 km/s (99% speed of light)
    SATELLITE     // ~300,000 km/s but higher latency overhead
};

/**
 * Latency Calculator - Haversine distance and speed-of-light delays
 */
class LatencyCalculator {
public:
    // Physical constants
    static constexpr double EARTH_RADIUS_KM = 6371.0;
    static constexpr double SPEED_OF_LIGHT_KM_MS = 299792.458; // km/ms
    static constexpr double FIBER_SPEED_FACTOR = 0.67;  // Fiber is 67% of c
    static constexpr double MICROWAVE_SPEED_FACTOR = 0.99; // Microwave ~99% of c
    
    /**
     * Calculate great-circle distance between two points using Haversine formula
     * @param lat1 Latitude of point 1 (degrees)
     * @param lon1 Longitude of point 1 (degrees)
     * @param lat2 Latitude of point 2 (degrees)
     * @param lon2 Longitude of point 2 (degrees)
     * @return Distance in kilometers
     */
    static double haversine_distance(double lat1, double lon1, double lat2, double lon2) {
        // Convert to radians
        double lat1_rad = to_radians(lat1);
        double lon1_rad = to_radians(lon1);
        double lat2_rad = to_radians(lat2);
        double lon2_rad = to_radians(lon2);
        
        // Haversine formula
        double dlat = lat2_rad - lat1_rad;
        double dlon = lon2_rad - lon1_rad;
        
        double a = std::sin(dlat / 2) * std::sin(dlat / 2) +
                   std::cos(lat1_rad) * std::cos(lat2_rad) *
                   std::sin(dlon / 2) * std::sin(dlon / 2);
        
        double c = 2 * std::atan2(std::sqrt(a), std::sqrt(1 - a));
        
        return EARTH_RADIUS_KM * c;
    }
    
    /**
     * Calculate distance between two exchanges
     */
    static double distance_between_exchanges(const Exchange& ex1, const Exchange& ex2) {
        return haversine_distance(ex1.latitude, ex1.longitude, 
                                  ex2.latitude, ex2.longitude);
    }
    
    /**
     * Calculate one-way latency based on distance and medium
     * @param distance_km Distance in kilometers
     * @param medium Transmission medium
     * @return Latency in milliseconds
     */
    static double calculate_latency(double distance_km, TransmissionMedium medium) {
        double speed_factor = (medium == TransmissionMedium::FIBER_OPTIC) 
                             ? FIBER_SPEED_FACTOR 
                             : MICROWAVE_SPEED_FACTOR;
        
        double effective_speed = SPEED_OF_LIGHT_KM_MS * speed_factor;
        double latency_ms = distance_km / effective_speed;
        
        // Add medium-specific overhead
        if (medium == TransmissionMedium::SATELLITE) {
            latency_ms += 250.0; // Geostationary satellite overhead
        }
        
        return latency_ms;
    }
    
    /**
     * Calculate round-trip time (RTT)
     */
    static double calculate_rtt(double distance_km, TransmissionMedium medium) {
        return 2.0 * calculate_latency(distance_km, medium);
    }
    
    /**
     * Calculate latency between two exchanges
     */
    static double latency_between_exchanges(const Exchange& ex1, const Exchange& ex2,
                                           TransmissionMedium medium = TransmissionMedium::FIBER_OPTIC) {
        double distance = distance_between_exchanges(ex1, ex2);
        return calculate_latency(distance, medium);
    }
    
    /**
     * Calculate if arbitrage is possible given latency window
     * @param price_diff Price difference between exchanges
     * @param latency_ms One-way latency in milliseconds
     * @param window_ms How long price discrepancy lasts (milliseconds)
     * @return true if arbitrage opportunity exists
     */
    static bool is_arbitrage_possible(double price_diff, double latency_ms, double window_ms) {
        // Need to receive price update, process, and send order before window closes
        double total_time_needed = latency_ms * 2; // RTT
        return total_time_needed < window_ms && price_diff > 0;
    }
    
private:
    static double to_radians(double degrees) {
        return degrees * M_PI / 180.0;
    }
    
    static double to_degrees(double radians) {
        return radians * 180.0 / M_PI;
    }
};

/**
 * Get transmission medium as string
 */
inline std::string medium_to_string(TransmissionMedium medium) {
    switch (medium) {
        case TransmissionMedium::FIBER_OPTIC: return "Fiber Optic";
        case TransmissionMedium::MICROWAVE: return "Microwave";
        case TransmissionMedium::SATELLITE: return "Satellite";
        default: return "Unknown";
    }
}