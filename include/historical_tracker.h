#pragma once

#include <vector>
#include <chrono>
#include "arbitrage_scanner.h"

/**
 * Snapshot of opportunities at a point in time
 */
struct OpportunitySnapshot {
    uint64_t timestamp;
    std::vector<ArbitrageOpportunity> opportunities;
    int total_count;
    int executable_count;
    double avg_profit;
    double max_profit;
};

/**
 * Historical Tracker
 * Records and replays arbitrage opportunities over time
 */
class HistoricalTracker {
private:
    std::vector<OpportunitySnapshot> history;
    size_t max_history_size;
    int current_playback_index;
    bool is_playing;
    
public:
    HistoricalTracker(size_t max_size = 600) : // 10 minutes at 1 snapshot/sec
        max_history_size(max_size), current_playback_index(0), is_playing(false) {}
    
    /**
     * Record current opportunities
     */
    void record(const std::vector<ArbitrageOpportunity>& opportunities) {
        OpportunitySnapshot snapshot;
        snapshot.timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()
        ).count();
        
        snapshot.opportunities = opportunities;
        snapshot.total_count = opportunities.size();
        snapshot.executable_count = 0;
        snapshot.avg_profit = 0;
        snapshot.max_profit = 0;
        
        for (const auto& opp : opportunities) {
            if (opp.is_executable) {
                snapshot.executable_count++;
            }
            snapshot.avg_profit += opp.estimated_profit;
            snapshot.max_profit = std::max(snapshot.max_profit, opp.estimated_profit);
        }
        
        if (!opportunities.empty()) {
            snapshot.avg_profit /= opportunities.size();
        }
        
        history.push_back(snapshot);
        
        // Maintain max size
        if (history.size() > max_history_size) {
            history.erase(history.begin());
        }
    }
    
    /**
     * Start playback
     */
    void startPlayback() {
        is_playing = true;
        current_playback_index = 0;
    }
    
    /**
     * Stop playback
     */
    void stopPlayback() {
        is_playing = false;
    }
    
    /**
     * Get next frame in playback
     */
    const OpportunitySnapshot* getNextFrame() {
        if (!is_playing || history.empty()) {
            return nullptr;
        }
        
        if (current_playback_index >= history.size()) {
            current_playback_index = 0; // Loop
        }
        
        return &history[current_playback_index++];
    }
    
    /**
     * Seek to specific time
     */
    void seekToIndex(int index) {
        if (index >= 0 && index < (int)history.size()) {
            current_playback_index = index;
        }
    }
    
    /**
     * Get current playback position
     */
    int getCurrentIndex() const { return current_playback_index; }
    int getTotalFrames() const { return history.size(); }
    bool isPlaying() const { return is_playing; }
    
    /**
     * Get statistics over time window
     */
    struct TimeWindowStats {
        int total_opportunities;
        int avg_opportunities_per_snapshot;
        double avg_profit;
        double total_potential_profit;
        int most_active_second;
    };
    
    TimeWindowStats getWindowStats(int last_n_seconds = 60) {
        TimeWindowStats stats{};
        stats.total_opportunities = 0;
        stats.avg_opportunities_per_snapshot = 0;
        stats.avg_profit = 0;
        stats.total_potential_profit = 0;
        stats.most_active_second = 0;
        
        if (history.empty()) return stats;
        
        int count = std::min(last_n_seconds, (int)history.size());
        int max_opps = 0;
        
        for (int i = history.size() - count; i < (int)history.size(); i++) {
            const auto& snap = history[i];
            stats.total_opportunities += snap.total_count;
            stats.avg_profit += snap.avg_profit;
            stats.total_potential_profit += snap.max_profit;
            
            if (snap.total_count > max_opps) {
                max_opps = snap.total_count;
                stats.most_active_second = i;
            }
        }
        
        if (count > 0) {
            stats.avg_opportunities_per_snapshot = stats.total_opportunities / count;
            stats.avg_profit /= count;
        }
        
        return stats;
    }
    
    /**
     * Clear history
     */
    void clear() {
        history.clear();
        current_playback_index = 0;
        is_playing = false;
    }
};