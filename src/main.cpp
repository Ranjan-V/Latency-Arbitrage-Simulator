#include <iostream>
#include <fstream>

// CRITICAL: Include GLAD before any OpenGL headers (GLFW, ImGui OpenGL backend)
#include <glad/glad.h>

#include <GLFW/glfw3.h>
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include <nlohmann/json.hpp>

#include "exchange.h"
#include "latency_calculator.h"
#include "network_graph.h"
#include "price_feed.h"
#include "arbitrage_scanner.h"
#include "globe_renderer.h"
#include "colocation_optimizer.h"
#include "historical_tracker.h"

using json = nlohmann::json;

// Global state
NetworkGraph g_network;
PriceFeed g_price_feed;
ArbitrageScanner* g_scanner = nullptr;
GlobeRenderer* g_globe_renderer = nullptr;
ColocationOptimizer* g_colocation_optimizer = nullptr;
HistoricalTracker* g_historical_tracker = nullptr;

std::string g_selected_exchange_1;
std::string g_selected_exchange_2;
TransmissionMedium g_transmission_medium = TransmissionMedium::FIBER_OPTIC;

// Arbitrage settings
float g_volatility = 0.02f;
float g_min_profit_bps = 5.0f;
float g_trading_fee = 0.1f;
float g_opportunity_window = 200.0f;
bool g_auto_inject_opportunities = false;
int g_update_counter = 0;

// Globe view settings
bool g_show_globe = true;
int g_globe_width = 800;
int g_globe_height = 600;
double g_mouse_x = 0;
double g_mouse_y = 0;

// Co-location settings
std::vector<std::string> g_target_exchanges;
bool g_show_colocation = false;

// Historical playback
bool g_show_historical = false;
int g_playback_speed = 1;

/**
 * Mouse button callback
 */
void mouse_button_callback(GLFWwindow* window, int button, int action, int mods) {
    if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_PRESS) {
        if (g_globe_renderer) {
            g_globe_renderer->handleClick();
        }
    }
}

/**
 * Mouse move callback
 */
void cursor_position_callback(GLFWwindow* window, double xpos, double ypos) {
    g_mouse_x = xpos;
    g_mouse_y = ypos;
    
    if (g_globe_renderer) {
        int width, height;
        glfwGetFramebufferSize(window, &width, &height);
        g_globe_renderer->updateMousePosition(xpos, ypos, width, height);
    }
}

// Performance tracking
struct TradingStats {
    double total_profit = 0;
    int total_trades = 0;
    int successful_trades = 0;
    double best_trade_profit = 0;
    std::string best_trade_route;
};
TradingStats g_trading_stats;

/**
 * Render Co-Location Optimizer UI
 */
void render_colocation_optimizer() {
    ImGui::Begin("🎯 Co-Location Optimizer", &g_show_colocation);
    
    ImGui::Text("Find optimal server placement");
    ImGui::Separator();
    
    // Target exchange selector
    ImGui::Text("Select target exchanges:");
    const auto& exchanges = g_network.get_exchanges();
    
    if (ImGui::BeginChild("ExchangeSelector", ImVec2(0, 150), true)) {
        for (const auto& ex : exchanges) {
            bool is_selected = std::find(g_target_exchanges.begin(), 
                                        g_target_exchanges.end(), 
                                        ex.id) != g_target_exchanges.end();
            
            if (ImGui::Checkbox(ex.id.c_str(), &is_selected)) {
                if (is_selected) {
                    g_target_exchanges.push_back(ex.id);
                } else {
                    g_target_exchanges.erase(
                        std::remove(g_target_exchanges.begin(), 
                                  g_target_exchanges.end(), ex.id),
                        g_target_exchanges.end());
                }
            }
            ImGui::SameLine();
            ImGui::TextDisabled("(%s)", ex.city.c_str());
        }
    }
    ImGui::EndChild();
    
    if (ImGui::Button("Clear Selection")) {
        g_target_exchanges.clear();
    }
    ImGui::SameLine();
    ImGui::Text("Selected: %zu exchanges", g_target_exchanges.size());
    
    ImGui::Separator();
    
    // Optimization results
    if (g_target_exchanges.size() >= 2 && g_colocation_optimizer) {
        auto result = g_colocation_optimizer->optimize(g_target_exchanges);
        
        if (!result.optimal_exchange_id.empty()) {
            ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), 
                             "Optimal Location: %s", result.optimal_exchange_id.c_str());
            
            const Exchange* optimal_ex = g_network.get_exchange(result.optimal_exchange_id);
            if (optimal_ex) {
                ImGui::Text("City: %s", optimal_ex->city.c_str());
            }
            
            ImGui::Separator();
            ImGui::Text("Total Latency: %.2f ms", result.total_latency);
            ImGui::Text("Average Latency: %.2f ms", result.avg_latency);
            ImGui::Text("Min Latency: %.2f ms", result.min_latency);
            ImGui::Text("Max Latency: %.2f ms", result.max_latency);
            ImGui::Text("Improvement: %.1f%% vs worst location", result.improvement_percent);
            
            ImGui::Separator();
            ImGui::Text("Latencies to targets:");
            
            if (ImGui::BeginTable("LatencyTable", 2, ImGuiTableFlags_Borders)) {
                ImGui::TableSetupColumn("Exchange");
                ImGui::TableSetupColumn("Latency (ms)");
                ImGui::TableHeadersRow();
                
                for (const auto& [target_id, latency] : result.latencies_to_targets) {
                    ImGui::TableNextRow();
                    ImGui::TableNextColumn();
                    ImGui::Text("%s", target_id.c_str());
                    ImGui::TableNextColumn();
                    ImGui::Text("%.2f ms", latency);
                }
                
                ImGui::EndTable();
            }
            
            // Highlight optimal location on globe
            if (g_globe_renderer && optimal_ex) {
                ImGui::Separator();
                
                // Find index of optimal exchange
                const auto& all_exchanges = g_network.get_exchanges();
                for (size_t i = 0; i < all_exchanges.size(); i++) {
                    if (all_exchanges[i].id == result.optimal_exchange_id) {
                        // This will be shown on globe as selected
                        ImGui::TextColored(ImVec4(1.0f, 0.0f, 1.0f, 1.0f), 
                                         "✓ %s highlighted in MAGENTA on globe", 
                                         result.optimal_exchange_id.c_str());
                        ImGui::TextDisabled("(The magenta/pink dot shows optimal location)");
                        break;
                    }
                }
            }
        }
    } else if (g_target_exchanges.size() < 2) {
        ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.0f, 1.0f), 
                         "Select at least 2 exchanges");
    }
    
    ImGui::End();
}

/**
 * Render Historical Playback UI
 */
void render_historical_playback() {
    ImGui::Begin("⏮️ Historical Playback", &g_show_historical);
    
    if (!g_historical_tracker) {
        ImGui::Text("Historical tracking not available");
        ImGui::End();
        return;
    }
    
    ImGui::Text("Recorded frames: %d", g_historical_tracker->getTotalFrames());
    ImGui::Text("Current position: %d", g_historical_tracker->getCurrentIndex());
    
    ImGui::Separator();
    
    // Playback controls
    if (g_historical_tracker->isPlaying()) {
        if (ImGui::Button("⏸ Pause")) {
            g_historical_tracker->stopPlayback();
        }
    } else {
        if (ImGui::Button("▶ Play")) {
            g_historical_tracker->startPlayback();
        }
    }
    
    ImGui::SameLine();
    if (ImGui::Button("⏹ Stop")) {
        g_historical_tracker->stopPlayback();
        g_historical_tracker->seekToIndex(0);
    }
    
    ImGui::SameLine();
    if (ImGui::Button("Clear History")) {
        g_historical_tracker->clear();
    }
    
    // Playback speed
    ImGui::SliderInt("Speed", &g_playback_speed, 1, 10, "%dx");
    
    // Seek slider
    int total_frames = g_historical_tracker->getTotalFrames();
    if (total_frames > 0) {
        int current = g_historical_tracker->getCurrentIndex();
        if (ImGui::SliderInt("Position", &current, 0, total_frames - 1)) {
            g_historical_tracker->seekToIndex(current);
        }
    }
    
    ImGui::Separator();
    
    // Statistics
    auto stats = g_historical_tracker->getWindowStats(60);
    ImGui::Text("Last 60 seconds:");
    ImGui::Text("Total Opportunities: %d", stats.total_opportunities);
    ImGui::Text("Avg per Second: %d", stats.avg_opportunities_per_snapshot);
    ImGui::Text("Avg Profit: $%.2f", stats.avg_profit);
    ImGui::Text("Total Potential: $%.2f", stats.total_potential_profit);
    
    ImGui::End();
}

/**
 * Render Performance Metrics UI
 */
void render_performance_metrics() {
    ImGui::SetNextWindowPos(ImVec2(10, 150), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(250, 150), ImGuiCond_FirstUseEver);
    ImGui::Begin("📊 Performance", nullptr, ImGuiWindowFlags_NoResize);
    
    ImGuiIO& io = ImGui::GetIO();
    ImGui::Text("FPS: %.1f", io.Framerate);
    ImGui::Text("Frame Time: %.2f ms", 1000.0f / io.Framerate);
    
    ImGui::Separator();
    
    // Network stats
    auto network_stats = g_network.get_statistics();
    ImGui::Text("Exchanges: %d", network_stats.num_exchanges);
    ImGui::Text("Connections: %d", network_stats.num_connections);
    ImGui::Text("Avg Latency: %.2f ms", network_stats.avg_latency_ms);
    
    ImGui::Separator();
    
    // Scanner stats
    if (g_scanner) {
        auto scanner_stats = g_scanner->get_statistics();
        ImGui::Text("Opportunities: %d", scanner_stats.total_opportunities);
        ImGui::Text("Executable: %d", scanner_stats.executable_opportunities);
    }
    
    ImGui::End();
}

/**
 * Render Exchange Info Tooltip
 */
void render_exchange_tooltip() {
    if (!g_globe_renderer) return;
    
    int hovered = g_globe_renderer->getHoveredExchange();
    
    if (hovered >= 0) {
        const auto& exchanges = g_network.get_exchanges();
        if (hovered < (int)exchanges.size()) {
            const auto& ex = exchanges[hovered];
            const auto& prices = g_price_feed.get_all_prices();
            auto price_it = prices.find(ex.id);
            
            // Position tooltip near mouse
            ImGui::SetNextWindowPos(ImVec2(g_mouse_x + 15, g_mouse_y + 15));
            ImGui::SetNextWindowBgAlpha(0.9f);
            
            ImGui::Begin("##ExchangeTooltip", nullptr, 
                        ImGuiWindowFlags_NoDecoration | 
                        ImGuiWindowFlags_AlwaysAutoResize |
                        ImGuiWindowFlags_NoSavedSettings |
                        ImGuiWindowFlags_NoFocusOnAppearing |
                        ImGuiWindowFlags_NoNav);
            
            ImGui::TextColored(ImVec4(0.0f, 1.0f, 1.0f, 1.0f), "%s", ex.name.c_str());
            ImGui::Separator();
            ImGui::Text("ID: %s", ex.id.c_str());
            ImGui::Text("City: %s", ex.city.c_str());
            ImGui::Text("Type: %s", ex.get_type_string().c_str());
            ImGui::Text("Location: %.2f°, %.2f°", ex.latitude, ex.longitude);
            
            if (price_it != prices.end()) {
                ImGui::Separator();
                ImGui::Text("Bid: $%.2f", price_it->second.bid);
                ImGui::Text("Ask: $%.2f", price_it->second.ask);
                ImGui::Text("Spread: %.2f bps", 
                          (price_it->second.spread() / price_it->second.mid_price()) * 10000);
            }
            
            ImGui::Text(" ");
            ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), "Click to select");
            
            ImGui::End();
        }
    }
}

/**
 * Render 3D Globe View (now just shows controls)
 */
void render_globe_view() {
    if (!g_show_globe || !g_globe_renderer) return;
    
    ImGui::SetNextWindowPos(ImVec2(10, 10), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(300, 120), ImGuiCond_FirstUseEver);
    ImGui::Begin("🌍 Globe Controls", &g_show_globe, ImGuiWindowFlags_NoResize);
    
    ImGui::Text("Globe renders as background");
    ImGui::Separator();
    
    if (ImGui::Button("Zoom In [+]", ImVec2(90, 30))) {
        g_globe_renderer->zoomIn();
    }
    ImGui::SameLine();
    if (ImGui::Button("Zoom Out [-]", ImVec2(90, 30))) {
        g_globe_renderer->zoomOut();
    }
    ImGui::SameLine();
    if (ImGui::Button("Reset [R]", ImVec2(90, 30))) {
        g_globe_renderer->resetCamera();
    }
    
    ImGui::Text("Auto-rotating 3D Earth");
    
    ImGui::End();
}

/**
 * Load exchanges from JSON file
 */
bool load_exchanges(const std::string& filepath, NetworkGraph& network) {
    std::ifstream file(filepath);
    if (!file.is_open()) {
        std::cerr << "Failed to open: " << filepath << std::endl;
        return false;
    }
    
    json data;
    file >> data;
    
    if (!data.contains("exchanges")) {
        std::cerr << "Invalid JSON: missing 'exchanges' field" << std::endl;
        return false;
    }
    
    for (const auto& ex_json : data["exchanges"]) {
        std::string id = ex_json["id"];
        std::string name = ex_json["name"];
        std::string city = ex_json["city"];
        double lat = ex_json["lat"];
        double lon = ex_json["lon"];
        std::string type_str = ex_json["type"];
        
        ExchangeType type = string_to_exchange_type(type_str);
        Exchange ex(id, name, city, lat, lon, type);
        network.add_exchange(ex);
    }
    
    std::cout << "Loaded " << network.get_exchanges().size() << " exchanges" << std::endl;
    return true;
}

/**
 * Render Exchange Table UI
 */
void render_exchange_table() {
    ImGui::Begin("Exchange Network", nullptr, ImGuiWindowFlags_AlwaysAutoResize);
    
    const auto& exchanges = g_network.get_exchanges();
    const auto& prices = g_price_feed.get_all_prices();
    
    ImGui::Text("Total Exchanges: %zu", exchanges.size());
    ImGui::Separator();
    
    if (ImGui::BeginTable("ExchangeTable", 7, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY, ImVec2(0, 300))) {
        ImGui::TableSetupColumn("ID");
        ImGui::TableSetupColumn("Name");
        ImGui::TableSetupColumn("City");
        ImGui::TableSetupColumn("Type");
        ImGui::TableSetupColumn("Bid");
        ImGui::TableSetupColumn("Ask");
        ImGui::TableSetupColumn("Spread");
        ImGui::TableHeadersRow();
        
        for (const auto& ex : exchanges) {
            auto price_it = prices.find(ex.id);
            
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::Text("%s", ex.id.c_str());
            
            ImGui::TableNextColumn();
            ImGui::Text("%s", ex.name.c_str());
            
            ImGui::TableNextColumn();
            ImGui::Text("%s", ex.city.c_str());
            
            ImGui::TableNextColumn();
            ImGui::Text("%s", ex.get_type_string().c_str());
            
            if (price_it != prices.end()) {
                const auto& quote = price_it->second;
                ImGui::TableNextColumn();
                ImGui::Text("$%.2f", quote.bid);
                
                ImGui::TableNextColumn();
                ImGui::Text("$%.2f", quote.ask);
                
                ImGui::TableNextColumn();
                ImGui::Text("%.2f bps", (quote.spread() / quote.mid_price()) * 10000);
            } else {
                ImGui::TableNextColumn();
                ImGui::Text("N/A");
                ImGui::TableNextColumn();
                ImGui::Text("N/A");
                ImGui::TableNextColumn();
                ImGui::Text("N/A");
            }
        }
        
        ImGui::EndTable();
    }
    
    ImGui::End();
}

/**
 * Render Latency Calculator UI
 */
void render_latency_calculator() {
    ImGui::Begin("Latency Calculator", nullptr, ImGuiWindowFlags_AlwaysAutoResize);
    
    const auto& exchanges = g_network.get_exchanges();
    
    // Exchange selector 1
    if (ImGui::BeginCombo("Exchange 1", g_selected_exchange_1.c_str())) {
        for (const auto& ex : exchanges) {
            bool is_selected = (g_selected_exchange_1 == ex.id);
            if (ImGui::Selectable(ex.id.c_str(), is_selected)) {
                g_selected_exchange_1 = ex.id;
            }
            if (is_selected) {
                ImGui::SetItemDefaultFocus();
            }
        }
        ImGui::EndCombo();
    }
    
    // Exchange selector 2
    if (ImGui::BeginCombo("Exchange 2", g_selected_exchange_2.c_str())) {
        for (const auto& ex : exchanges) {
            bool is_selected = (g_selected_exchange_2 == ex.id);
            if (ImGui::Selectable(ex.id.c_str(), is_selected)) {
                g_selected_exchange_2 = ex.id;
            }
            if (is_selected) {
                ImGui::SetItemDefaultFocus();
            }
        }
        ImGui::EndCombo();
    }
    
    // Transmission medium selector
    const char* medium_items[] = { "Fiber Optic", "Microwave", "Satellite" };
    int medium_current = static_cast<int>(g_transmission_medium);
    if (ImGui::Combo("Transmission Medium", &medium_current, medium_items, 3)) {
        g_transmission_medium = static_cast<TransmissionMedium>(medium_current);
    }
    
    ImGui::Separator();
    
    // Calculate and display results
    if (!g_selected_exchange_1.empty() && !g_selected_exchange_2.empty()) {
        const Exchange* ex1 = g_network.get_exchange(g_selected_exchange_1);
        const Exchange* ex2 = g_network.get_exchange(g_selected_exchange_2);
        
        if (ex1 && ex2) {
            double distance = LatencyCalculator::distance_between_exchanges(*ex1, *ex2);
            double latency = LatencyCalculator::calculate_latency(distance, g_transmission_medium);
            double rtt = LatencyCalculator::calculate_rtt(distance, g_transmission_medium);
            
            ImGui::Text("Distance: %.2f km", distance);
            ImGui::Text("One-way Latency: %.3f ms", latency);
            ImGui::Text("Round-trip Time: %.3f ms", rtt);
            
            double light_speed_latency = distance / LatencyCalculator::SPEED_OF_LIGHT_KM_MS;
            ImGui::Text("Theoretical Min (c): %.3f ms", light_speed_latency);
        }
    }
    
    ImGui::End();
}

/**
 * Render Network Statistics UI
 */
void render_network_stats() {
    ImGui::Begin("Network Statistics", nullptr, ImGuiWindowFlags_AlwaysAutoResize);
    
    auto stats = g_network.get_statistics();
    
    ImGui::Text("Exchanges: %d", stats.num_exchanges);
    ImGui::Text("Connections: %d", stats.num_connections);
    ImGui::Separator();
    ImGui::Text("Avg Distance: %.2f km", stats.avg_distance_km);
    ImGui::Text("Avg Latency: %.3f ms", stats.avg_latency_ms);
    ImGui::Text("Min Latency: %.3f ms", stats.min_latency_ms);
    ImGui::Text("Max Latency: %.3f ms", stats.max_latency_ms);
    
    ImGui::End();
}

/**
 * Render Arbitrage Opportunities UI
 */
void render_arbitrage_opportunities() {
    ImGui::Begin("🔥 Arbitrage Opportunities", nullptr, ImGuiWindowFlags_None);
    
    ImGui::Text("Live Arbitrage Scanner");
    ImGui::Separator();
    
    // Controls
    ImGui::SliderFloat("Volatility", &g_volatility, 0.0f, 0.1f, "%.4f");
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Higher = more price movement");
    }
    
    ImGui::SliderFloat("Min Profit (bps)", &g_min_profit_bps, 1.0f, 50.0f);
    ImGui::SliderFloat("Trading Fee (%)", &g_trading_fee, 0.0f, 1.0f, "%.2f");
    ImGui::SliderFloat("Opportunity Window (ms)", &g_opportunity_window, 50.0f, 1000.0f);
    
    ImGui::Checkbox("Auto-inject Opportunities", &g_auto_inject_opportunities);
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Automatically create price discrepancies for testing");
    }
    
    if (ImGui::Button("Manual Price Update")) {
        g_price_feed.update_prices();
    }
    ImGui::SameLine();
    if (ImGui::Button("Inject Arbitrage")) {
        const auto& exchanges = g_network.get_exchanges();
        if (!exchanges.empty()) {
            int random_idx = rand() % exchanges.size();
            g_price_feed.inject_arbitrage_opportunity(exchanges[random_idx].id, 0.5);
        }
    }
    
    // Update scanner settings
    if (g_scanner) {
        g_scanner->set_min_profit_bps(g_min_profit_bps);
        g_scanner->set_trading_fee(g_trading_fee);
        g_scanner->set_opportunity_window(g_opportunity_window);
    }
    
    g_price_feed.set_volatility(g_volatility);
    
    ImGui::Separator();
    
    // Get opportunities
    auto opportunities = g_scanner ? g_scanner->get_top_opportunities(20) : std::vector<ArbitrageOpportunity>();
    
    ImGui::Text("Found %zu opportunities", opportunities.size());
    
    // Opportunities table
    if (ImGui::BeginTable("OpportunitiesTable", 8, 
        ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY, 
        ImVec2(0, 400))) {
        
        ImGui::TableSetupColumn("Buy");
        ImGui::TableSetupColumn("Sell");
        ImGui::TableSetupColumn("Profit %");
        ImGui::TableSetupColumn("Est. Profit $");
        ImGui::TableSetupColumn("Latency");
        ImGui::TableSetupColumn("RTT");
        ImGui::TableSetupColumn("Window");
        ImGui::TableSetupColumn("Status");
        ImGui::TableHeadersRow();
        
        for (const auto& opp : opportunities) {
            ImGui::TableNextRow();
            
            // Color code by profitability
            if (opp.is_executable) {
                ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0, 
                    ImGui::GetColorU32(ImVec4(0.0f, 0.3f, 0.0f, 0.3f)));
            }
            
            ImGui::TableNextColumn();
            ImGui::Text("%s", opp.buy_exchange.c_str());
            
            ImGui::TableNextColumn();
            ImGui::Text("%s", opp.sell_exchange.c_str());
            
            ImGui::TableNextColumn();
            ImGui::Text("%.3f%%", opp.profit_percent);
            
            ImGui::TableNextColumn();
            ImGui::Text("$%.2f", opp.estimated_profit);
            
            ImGui::TableNextColumn();
            ImGui::Text("%.1f ms", opp.latency_ms);
            
            ImGui::TableNextColumn();
            ImGui::Text("%.1f ms", opp.rtt_ms);
            
            ImGui::TableNextColumn();
            ImGui::Text("%.0f ms", opp.opportunity_window_ms);
            
            ImGui::TableNextColumn();
            if (opp.is_executable) {
                ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "✓ GO");
                ImGui::SameLine();
                if (ImGui::SmallButton(("Execute##" + opp.buy_exchange + opp.sell_exchange).c_str())) {
                    // Simulate trade execution
                    g_trading_stats.total_trades++;
                    
                    // Add profit
                    g_trading_stats.total_profit += opp.estimated_profit;
                    
                    // Count as successful if profit is positive
                    if (opp.estimated_profit > 0) {
                        g_trading_stats.successful_trades++;
                        
                        // Update best trade
                        if (opp.estimated_profit > g_trading_stats.best_trade_profit) {
                            g_trading_stats.best_trade_profit = opp.estimated_profit;
                            g_trading_stats.best_trade_route = opp.buy_exchange + " → " + opp.sell_exchange;
                        }
                    }
                    
                    std::cout << "✓ Executed trade: " << opp.buy_exchange << " -> " << opp.sell_exchange 
                              << " | Profit: $" << opp.estimated_profit << std::endl;
                }
            } else {
                ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "✗ SLOW");
            }
        }
        
        ImGui::EndTable();
    }
    
    ImGui::End();
}

/**
 * Render Trading Statistics UI
 */
void render_trading_stats() {
    ImGui::Begin("💰 Trading Statistics", nullptr, ImGuiWindowFlags_AlwaysAutoResize);
    
    ImGui::Text("Total Trades: %d", g_trading_stats.total_trades);
    ImGui::Text("Successful: %d", g_trading_stats.successful_trades);
    
    if (g_trading_stats.total_trades > 0) {
        float success_rate = (float)g_trading_stats.successful_trades / g_trading_stats.total_trades * 100.0f;
        ImGui::Text("Success Rate: %.1f%%", success_rate);
    }
    
    ImGui::Separator();
    ImGui::Text("Total P&L: $%.2f", g_trading_stats.total_profit);
    ImGui::Text("Best Trade: $%.2f", g_trading_stats.best_trade_profit);
    if (!g_trading_stats.best_trade_route.empty()) {
        ImGui::Text("Best Route: %s", g_trading_stats.best_trade_route.c_str());
    }
    
    if (ImGui::Button("Reset Stats")) {
        g_trading_stats = TradingStats();
    }
    
    ImGui::End();
}

int main() {
    std::cout << "Latency Arbitrage Simulator - Initializing..." << std::endl;
    
    // Initialize GLFW
    if (!glfwInit()) {
        std::cerr << "Failed to initialize GLFW" << std::endl;
        return -1;
    }
    
    // Create window
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    
    GLFWwindow* window = glfwCreateWindow(1920, 1080, "🚀 Latency Arbitrage Simulator", nullptr, nullptr);
    if (!window) {
        std::cerr << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return -1;
    }
    
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1); // Enable vsync
    
    // Set mouse callbacks
    glfwSetMouseButtonCallback(window, mouse_button_callback);
    glfwSetCursorPosCallback(window, cursor_position_callback);
    
    // Initialize GLAD (MUST be after glfwMakeContextCurrent)
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        std::cerr << "Failed to initialize GLAD" << std::endl;
        return -1;
    }
    
    std::cout << "OpenGL Version: " << glGetString(GL_VERSION) << std::endl;
    std::cout << "GLSL Version: " << glGetString(GL_SHADING_LANGUAGE_VERSION) << std::endl;
    
    // Initialize Dear ImGui
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    ImGui::StyleColorsDark();
    
    // Customize ImGui style
    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowRounding = 5.0f;
    style.FrameRounding = 3.0f;
    style.Colors[ImGuiCol_WindowBg] = ImVec4(0.1f, 0.1f, 0.12f, 0.95f);
    style.Colors[ImGuiCol_TitleBgActive] = ImVec4(0.2f, 0.4f, 0.8f, 1.0f);
    
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 330");
    
    // Load exchange data
    if (!load_exchanges("../data/exchanges.json", g_network)) {
        std::cerr << "Failed to load exchanges!" << std::endl;
        return -1;
    }
    
    // Build network connections
    g_network.connect_all_exchanges(TransmissionMedium::FIBER_OPTIC);
    std::cout << "Network graph built successfully!" << std::endl;
    
    // Initialize price feeds
    g_price_feed.initialize_feeds(g_network.get_exchanges());
    std::cout << "Price feeds initialized!" << std::endl;
    
    // Initialize arbitrage scanner
    g_scanner = new ArbitrageScanner(g_network, g_price_feed);
    std::cout << "Arbitrage scanner ready!" << std::endl;
    
    // Initialize co-location optimizer
    g_colocation_optimizer = new ColocationOptimizer(g_network);
    std::cout << "Co-location optimizer ready!" << std::endl;
    
    // Initialize historical tracker
    g_historical_tracker = new HistoricalTracker(600); // 10 minutes
    std::cout << "Historical tracker ready!" << std::endl;
    
    // Initialize globe renderer
    g_globe_renderer = new GlobeRenderer();
    if (!g_globe_renderer->initialize()) {
        std::cerr << "Failed to initialize globe renderer!" << std::endl;
        return -1;
    }
    std::cout << "Globe renderer initialized!" << std::endl;
    
    // Initialize selections
    if (!g_network.get_exchanges().empty()) {
        g_selected_exchange_1 = g_network.get_exchanges()[0].id;
        if (g_network.get_exchanges().size() > 1) {
            g_selected_exchange_2 = g_network.get_exchanges()[1].id;
        }
    }
    
    std::cout << "Setup complete! Window created." << std::endl;
    
    // Main loop
    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();
        
        // Update prices periodically
        g_update_counter++;
        if (g_update_counter % 60 == 0) {  // Every 60 frames (~1 second at 60 FPS)
            g_price_feed.update_prices();
            
            // Record historical data
            if (g_historical_tracker) {
                auto opportunities = g_scanner->scan_opportunities();
                g_historical_tracker->record(opportunities);
            }
            
            // Auto-inject opportunities for demo
            if (g_auto_inject_opportunities && g_update_counter % 180 == 0) {
                const auto& exchanges = g_network.get_exchanges();
                if (!exchanges.empty()) {
                    int random_idx = rand() % exchanges.size();
                    double deviation = (rand() % 100) / 100.0;
                    g_price_feed.inject_arbitrage_opportunity(exchanges[random_idx].id, deviation);
                }
            }
        }
        
        // Start ImGui frame
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
        
        // RENDER GLOBE FIRST (before ImGui UI)
        if (g_show_globe && g_globe_renderer) {
            int display_w, display_h;
            glfwGetFramebufferSize(window, &display_w, &display_h);
            
            // Render globe to full screen background
            glViewport(0, 0, display_w, display_h);
            auto opportunities = g_scanner ? g_scanner->get_top_opportunities(10) : std::vector<ArbitrageOpportunity>();
            g_globe_renderer->render(g_network.get_exchanges(), opportunities, display_w, display_h, true);
        }
        
        // Now render UI on top
        render_exchange_table();
        render_latency_calculator();
        render_network_stats();
        render_arbitrage_opportunities();
        render_trading_stats();
        render_globe_view();
        render_performance_metrics();
        render_exchange_tooltip();
        
        // Optional windows
        if (g_show_colocation) {
            render_colocation_optimizer();
        }
        if (g_show_historical) {
            render_historical_playback();
        }
        
        // Main menu bar
        if (ImGui::BeginMainMenuBar()) {
            if (ImGui::BeginMenu("View")) {
                ImGui::MenuItem("Globe", nullptr, &g_show_globe);
                ImGui::MenuItem("Co-Location Optimizer", nullptr, &g_show_colocation);
                ImGui::MenuItem("Historical Playback", nullptr, &g_show_historical);
                ImGui::EndMenu();
            }
            ImGui::EndMainMenuBar();
        }
        
        // Render ImGui
        ImGui::Render();
        
        // Render ImGui draw data on top of globe
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        
        glfwSwapBuffers(window);
    }
    
    // Cleanup
    delete g_scanner;
    delete g_globe_renderer;
    delete g_colocation_optimizer;
    delete g_historical_tracker;
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    glfwDestroyWindow(window);
    glfwTerminate();
    
    return 0;
}