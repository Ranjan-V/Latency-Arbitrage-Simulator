#pragma once

#include <vector>
#include <map>
#include <string>
#include <limits>
#include <algorithm>
#include "exchange.h"
#include "latency_calculator.h"

/**
 * Edge in the network graph (connection between exchanges)
 */
struct NetworkEdge {
    std::string from_exchange;
    std::string to_exchange;
    double distance_km;
    double latency_ms;
    TransmissionMedium medium;
    
    NetworkEdge(const std::string& from, const std::string& to, 
                double dist, double lat, TransmissionMedium med)
        : from_exchange(from), to_exchange(to), 
          distance_km(dist), latency_ms(lat), medium(med) {}
};

/**
 * Network Graph representing exchange connections
 */
class NetworkGraph {
private:
    std::vector<Exchange> exchanges;
    std::vector<NetworkEdge> edges;
    std::map<std::string, int> exchange_index_map; // ID -> index
    
public:
    /**
     * Add an exchange to the network
     */
    void add_exchange(const Exchange& exchange) {
        exchange_index_map[exchange.id] = exchanges.size();
        exchanges.push_back(exchange);
    }
    
    /**
     * Connect all exchanges (complete graph)
     * In reality, not all exchanges are directly connected
     */
    void connect_all_exchanges(TransmissionMedium medium = TransmissionMedium::FIBER_OPTIC) {
        edges.clear();
        
        for (size_t i = 0; i < exchanges.size(); i++) {
            for (size_t j = i + 1; j < exchanges.size(); j++) {
                double distance = LatencyCalculator::distance_between_exchanges(
                    exchanges[i], exchanges[j]
                );
                double latency = LatencyCalculator::calculate_latency(distance, medium);
                
                // Add bidirectional edges
                edges.emplace_back(exchanges[i].id, exchanges[j].id, 
                                  distance, latency, medium);
                edges.emplace_back(exchanges[j].id, exchanges[i].id, 
                                  distance, latency, medium);
            }
        }
    }
    
    /**
     * Get exchange by ID
     */
    const Exchange* get_exchange(const std::string& id) const {
        auto it = exchange_index_map.find(id);
        if (it != exchange_index_map.end()) {
            return &exchanges[it->second];
        }
        return nullptr;
    }
    
    /**
     * Get all exchanges
     */
    const std::vector<Exchange>& get_exchanges() const {
        return exchanges;
    }
    
    /**
     * Get all edges
     */
    const std::vector<NetworkEdge>& get_edges() const {
        return edges;
    }
    
    /**
     * Dijkstra's shortest path algorithm
     * @param start_id Starting exchange ID
     * @param end_id Destination exchange ID
     * @return Total latency in milliseconds (or infinity if no path)
     */
    double shortest_path_latency(const std::string& start_id, const std::string& end_id) const {
        // Simple implementation - for complete graph, direct path is shortest
        for (const auto& edge : edges) {
            if (edge.from_exchange == start_id && edge.to_exchange == end_id) {
                return edge.latency_ms;
            }
        }
        return std::numeric_limits<double>::infinity();
    }
    
    /**
     * Find optimal co-location point
     * Returns exchange ID that minimizes total latency to target exchanges
     * @param target_exchanges List of exchange IDs to optimize for
     * @return ID of optimal exchange location
     */
    std::string find_optimal_colocation(const std::vector<std::string>& target_exchanges) const {
        std::string best_location;
        double min_total_latency = std::numeric_limits<double>::infinity();
        
        // Try each exchange as potential co-location point
        for (const auto& candidate : exchanges) {
            double total_latency = 0.0;
            
            // Sum latencies to all target exchanges
            for (const auto& target_id : target_exchanges) {
                double latency = shortest_path_latency(candidate.id, target_id);
                if (std::isinf(latency)) {
                    total_latency = std::numeric_limits<double>::infinity();
                    break;
                }
                total_latency += latency;
            }
            
            if (total_latency < min_total_latency) {
                min_total_latency = total_latency;
                best_location = candidate.id;
            }
        }
        
        return best_location;
    }
    
    /**
     * Calculate average latency from one exchange to all others
     */
    double average_latency_from(const std::string& exchange_id) const {
        double total = 0.0;
        int count = 0;
        
        for (const auto& other : exchanges) {
            if (other.id != exchange_id) {
                double latency = shortest_path_latency(exchange_id, other.id);
                if (!std::isinf(latency)) {
                    total += latency;
                    count++;
                }
            }
        }
        
        return (count > 0) ? (total / count) : 0.0;
    }
    
    /**
     * Get statistics about the network
     */
    struct NetworkStats {
        int num_exchanges;
        int num_connections;
        double avg_distance_km;
        double avg_latency_ms;
        double max_latency_ms;
        double min_latency_ms;
    };
    
    NetworkStats get_statistics() const {
        NetworkStats stats;
        stats.num_exchanges = exchanges.size();
        stats.num_connections = edges.size() / 2; // Bidirectional
        
        if (edges.empty()) {
            stats.avg_distance_km = 0;
            stats.avg_latency_ms = 0;
            stats.max_latency_ms = 0;
            stats.min_latency_ms = 0;
            return stats;
        }
        
        double total_distance = 0;
        double total_latency = 0;
        stats.max_latency_ms = 0;
        stats.min_latency_ms = std::numeric_limits<double>::infinity();
        
        // Only count one direction of each edge
        for (size_t i = 0; i < edges.size(); i += 2) {
            total_distance += edges[i].distance_km;
            total_latency += edges[i].latency_ms;
            stats.max_latency_ms = std::max(stats.max_latency_ms, edges[i].latency_ms);
            stats.min_latency_ms = std::min(stats.min_latency_ms, edges[i].latency_ms);
        }
        
        stats.avg_distance_km = total_distance / stats.num_connections;
        stats.avg_latency_ms = total_latency / stats.num_connections;
        
        return stats;
    }
};