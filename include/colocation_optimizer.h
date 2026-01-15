#pragma once

#include <vector>
#include <string>
#include <map>
#include <limits>
#include "exchange.h"
#include "network_graph.h"

/**
 * Result of co-location optimization
 */
struct ColocationResult {
    std::string optimal_exchange_id;
    double total_latency;
    double avg_latency;
    double max_latency;
    double min_latency;
    std::map<std::string, double> latencies_to_targets;
    double improvement_percent; // vs worst location
};

/**
 * Co-Location Optimizer
 * Finds optimal server placement to minimize latency to target exchanges
 */
class ColocationOptimizer {
private:
    const NetworkGraph& network;
    
public:
    ColocationOptimizer(const NetworkGraph& net) : network(net) {}
    
    /**
     * Find optimal co-location point for given target exchanges
     */
    ColocationResult optimize(const std::vector<std::string>& target_exchange_ids) {
        ColocationResult result;
        result.optimal_exchange_id = "";
        result.total_latency = std::numeric_limits<double>::infinity();
        result.avg_latency = 0;
        result.max_latency = 0;
        result.min_latency = std::numeric_limits<double>::infinity();
        result.improvement_percent = 0;
        
        if (target_exchange_ids.empty()) {
            return result;
        }
        
        const auto& exchanges = network.get_exchanges();
        
        double worst_total_latency = 0;
        
        // Try each exchange as a potential co-location point
        for (const auto& candidate : exchanges) {
            double total_latency = 0;
            double max_lat = 0;
            double min_lat = std::numeric_limits<double>::infinity();
            bool valid = true;
            
            // Calculate total latency to all targets
            for (const auto& target_id : target_exchange_ids) {
                double latency = network.shortest_path_latency(candidate.id, target_id);
                
                if (std::isinf(latency)) {
                    valid = false;
                    break;
                }
                
                total_latency += latency;
                max_lat = std::max(max_lat, latency);
                min_lat = std::min(min_lat, latency);
            }
            
            if (!valid) continue;
            
            // Track worst case for improvement calculation
            worst_total_latency = std::max(worst_total_latency, total_latency);
            
            // Update result if this is better
            if (total_latency < result.total_latency) {
                result.optimal_exchange_id = candidate.id;
                result.total_latency = total_latency;
                result.avg_latency = total_latency / target_exchange_ids.size();
                result.max_latency = max_lat;
                result.min_latency = min_lat;
                
                // Store individual latencies
                result.latencies_to_targets.clear();
                for (const auto& target_id : target_exchange_ids) {
                    result.latencies_to_targets[target_id] = 
                        network.shortest_path_latency(candidate.id, target_id);
                }
            }
        }
        
        // Calculate improvement percentage
        if (worst_total_latency > 0 && result.total_latency < worst_total_latency) {
            result.improvement_percent = 
                ((worst_total_latency - result.total_latency) / worst_total_latency) * 100.0;
        }
        
        return result;
    }
    
    /**
     * Get top N best locations
     */
    std::vector<ColocationResult> getTopLocations(
        const std::vector<std::string>& target_exchange_ids, 
        int top_n = 5) {
        
        std::vector<ColocationResult> results;
        const auto& exchanges = network.get_exchanges();
        
        for (const auto& candidate : exchanges) {
            ColocationResult result;
            result.optimal_exchange_id = candidate.id;
            result.total_latency = 0;
            bool valid = true;
            
            for (const auto& target_id : target_exchange_ids) {
                double latency = network.shortest_path_latency(candidate.id, target_id);
                if (std::isinf(latency)) {
                    valid = false;
                    break;
                }
                result.total_latency += latency;
            }
            
            if (valid) {
                result.avg_latency = result.total_latency / target_exchange_ids.size();
                results.push_back(result);
            }
        }
        
        // Sort by total latency
        std::sort(results.begin(), results.end(),
                 [](const ColocationResult& a, const ColocationResult& b) {
                     return a.total_latency < b.total_latency;
                 });
        
        // Return top N
        if (results.size() > (size_t)top_n) {
            results.resize(top_n);
        }
        
        return results;
    }
};