#pragma once

#include <vector>
#include <string>
#include <fstream>
#include <sstream>
#include <iostream>
#include <cmath>
// NOTE: Do NOT include glad.h here - it's included in main.cpp before this header
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include "exchange.h"
#include "arbitrage_scanner.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/**
 * Shader program wrapper
 */
class Shader {
public:
    unsigned int ID;
    
    Shader(const char* vertexPath, const char* fragmentPath) {
        std::string vertexCode;
        std::string fragmentCode;
        std::ifstream vShaderFile;
        std::ifstream fShaderFile;
        
        vShaderFile.exceptions(std::ifstream::failbit | std::ifstream::badbit);
        fShaderFile.exceptions(std::ifstream::failbit | std::ifstream::badbit);
        
        try {
            vShaderFile.open(vertexPath);
            fShaderFile.open(fragmentPath);
            std::stringstream vShaderStream, fShaderStream;
            
            vShaderStream << vShaderFile.rdbuf();
            fShaderStream << fShaderFile.rdbuf();
            
            vShaderFile.close();
            fShaderFile.close();
            
            vertexCode = vShaderStream.str();
            fragmentCode = fShaderStream.str();
        } catch (std::ifstream::failure& e) {
            std::cerr << "ERROR: Shader file read failed: " << e.what() << std::endl;
        }
        
        const char* vShaderCode = vertexCode.c_str();
        const char* fShaderCode = fragmentCode.c_str();
        
        unsigned int vertex, fragment;
        
        // Vertex shader
        vertex = glCreateShader(GL_VERTEX_SHADER);
        glShaderSource(vertex, 1, &vShaderCode, NULL);
        glCompileShader(vertex);
        checkCompileErrors(vertex, "VERTEX");
        
        // Fragment shader
        fragment = glCreateShader(GL_FRAGMENT_SHADER);
        glShaderSource(fragment, 1, &fShaderCode, NULL);
        glCompileShader(fragment);
        checkCompileErrors(fragment, "FRAGMENT");
        
        // Shader program
        ID = glCreateProgram();
        glAttachShader(ID, vertex);
        glAttachShader(ID, fragment);
        glLinkProgram(ID);
        checkCompileErrors(ID, "PROGRAM");
        
        glDeleteShader(vertex);
        glDeleteShader(fragment);
    }
    
    void use() { glUseProgram(ID); }
    
    void setMat4(const std::string& name, const glm::mat4& mat) const {
        glUniformMatrix4fv(glGetUniformLocation(ID, name.c_str()), 1, GL_FALSE, &mat[0][0]);
    }
    
    void setVec3(const std::string& name, const glm::vec3& value) const {
        glUniform3fv(glGetUniformLocation(ID, name.c_str()), 1, &value[0]);
    }
    
    void setBool(const std::string& name, bool value) const {
        glUniform1i(glGetUniformLocation(ID, name.c_str()), (int)value);
    }
    
    void setFloat(const std::string& name, float value) const {
        glUniform1f(glGetUniformLocation(ID, name.c_str()), value);
    }
    
private:
    void checkCompileErrors(unsigned int shader, std::string type) {
        int success;
        char infoLog[1024];
        if (type != "PROGRAM") {
            glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
            if (!success) {
                glGetShaderInfoLog(shader, 1024, NULL, infoLog);
                std::cerr << "ERROR: Shader compilation failed (" << type << "): " << infoLog << std::endl;
            }
        } else {
            glGetProgramiv(shader, GL_LINK_STATUS, &success);
            if (!success) {
                glGetProgramInfoLog(shader, 1024, NULL, infoLog);
                std::cerr << "ERROR: Program linking failed: " << infoLog << std::endl;
            }
        }
    }
};

/**
 * Convert lat/lon to 3D Cartesian coordinates
 */
inline glm::vec3 latLonToCartesian(double lat, double lon, double radius = 1.0) {
    double lat_rad = lat * M_PI / 180.0;
    double lon_rad = lon * M_PI / 180.0;
    
    float x = radius * cos(lat_rad) * cos(lon_rad);
    float y = radius * sin(lat_rad);
    float z = radius * cos(lat_rad) * sin(lon_rad);
    
    return glm::vec3(x, y, z);
}

/**
 * Globe Renderer - 3D visualization
 */
class GlobeRenderer {
private:
    unsigned int sphereVAO, sphereVBO, sphereEBO;
    unsigned int markerVAO, markerVBO;
    unsigned int lineVAO, lineVBO;
    
    std::vector<float> sphereVertices;
    std::vector<unsigned int> sphereIndices;
    std::vector<float> lineVertices;
    
    // Cache for trade routes
    std::vector<ArbitrageOpportunity> cachedOpportunities;
    bool routesNeedUpdate;
    float routeAlpha; // Fade in/out alpha
    int framesSinceUpdate;
    
    // Interaction state
    int hoveredExchangeIndex;
    int selectedExchangeIndex;
    int selectedRouteIndex;
    
    // Mouse state
    double mouseX, mouseY;
    
    Shader* globeShader;
    Shader* lineShader;
    
    glm::vec3 cameraPos;
    glm::vec3 cameraTarget;
    float cameraDistance;
    float rotationAngle;
    
public:
    GlobeRenderer() : 
        sphereVAO(0), sphereVBO(0), sphereEBO(0),
        markerVAO(0), markerVBO(0), lineVAO(0), lineVBO(0),
        globeShader(nullptr), lineShader(nullptr),
        cameraPos(0.0f, 0.0f, 3.0f), cameraTarget(0.0f, 0.0f, 0.0f),
        cameraDistance(3.0f), rotationAngle(0.0f), routesNeedUpdate(true),
        routeAlpha(0.0f), framesSinceUpdate(0),
        hoveredExchangeIndex(-1), selectedExchangeIndex(-1), selectedRouteIndex(-1),
        mouseX(0), mouseY(0) {}
    
    ~GlobeRenderer() {
        cleanup();
    }
    
    /**
     * Initialize OpenGL resources
     */
    bool initialize() {
        // Load shaders
        globeShader = new Shader("../shaders/globe_vertex.glsl", "../shaders/globe_fragment.glsl");
        lineShader = new Shader("../shaders/line_vertex.glsl", "../shaders/line_fragment.glsl");
        
        // Generate sphere
        generateSphere(1.0f, 64, 64);
        
        // Setup sphere VAO
        glGenVertexArrays(1, &sphereVAO);
        glGenBuffers(1, &sphereVBO);
        glGenBuffers(1, &sphereEBO);
        
        glBindVertexArray(sphereVAO);
        
        glBindBuffer(GL_ARRAY_BUFFER, sphereVBO);
        glBufferData(GL_ARRAY_BUFFER, sphereVertices.size() * sizeof(float), 
                     sphereVertices.data(), GL_STATIC_DRAW);
        
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, sphereEBO);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, sphereIndices.size() * sizeof(unsigned int),
                     sphereIndices.data(), GL_STATIC_DRAW);
        
        // Position
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0);
        glEnableVertexAttribArray(0);
        
        // Normal
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(3 * sizeof(float)));
        glEnableVertexAttribArray(1);
        
        // TexCoord
        glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(6 * sizeof(float)));
        glEnableVertexAttribArray(2);
        
        // Setup marker VAO (reuse sphere but smaller)
        glGenVertexArrays(1, &markerVAO);
        glGenBuffers(1, &markerVBO);
        
        // Setup line VAO
        glGenVertexArrays(1, &lineVAO);
        glGenBuffers(1, &lineVBO);
        
        glBindVertexArray(lineVAO);
        glBindBuffer(GL_ARRAY_BUFFER, lineVBO);
        
        // Position + Color
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(3 * sizeof(float)));
        glEnableVertexAttribArray(1);
        
        glBindVertexArray(0);
        
        return true;
    }
    
    /**
     * Render the globe (now with proper viewport setup)
     */
    void render(const std::vector<Exchange>& exchanges, 
                const std::vector<ArbitrageOpportunity>& opportunities,
                int width, int height, bool clear_screen = true) {
        
        // Check if opportunities changed (to regenerate routes)
        if (opportunities.size() != cachedOpportunities.size()) {
            routesNeedUpdate = true;
            routeAlpha = 0.0f; // Start fade-in
            framesSinceUpdate = 0;
        } else {
            // Check if any opportunity changed
            for (size_t i = 0; i < opportunities.size(); i++) {
                if (i >= cachedOpportunities.size() ||
                    opportunities[i].buy_exchange != cachedOpportunities[i].buy_exchange ||
                    opportunities[i].sell_exchange != cachedOpportunities[i].sell_exchange) {
                    routesNeedUpdate = true;
                    routeAlpha = 0.0f; // Start fade-in
                    framesSinceUpdate = 0;
                    break;
                }
            }
        }
        
        // Update cached opportunities
        if (routesNeedUpdate) {
            cachedOpportunities = opportunities;
            generateTradeRoutes(exchanges, opportunities);
            routesNeedUpdate = false;
        }
        
        // Smooth fade-in animation
        framesSinceUpdate++;
        if (routeAlpha < 1.0f) {
            routeAlpha = std::min(1.0f, routeAlpha + 0.02f); // Fade in over 50 frames
        }
        
        if (clear_screen) {
            // Clear the screen for standalone rendering
            glClearColor(0.05f, 0.05f, 0.08f, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        }
        
        // Enable depth testing
        glEnable(GL_DEPTH_TEST);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glEnable(GL_LINE_SMOOTH);
        
        // Set viewport
        glViewport(0, 0, width, height);
        
        // Update camera rotation (SLOWED DOWN)
        rotationAngle += 0.05f;  // Changed from 0.2f to 0.05f (4x slower)
        if (rotationAngle > 360.0f) rotationAngle -= 360.0f;
        
        // Setup matrices
        glm::mat4 projection = glm::perspective(glm::radians(45.0f), 
                                               (float)width / (float)height, 
                                               0.1f, 100.0f);
        glm::mat4 view = glm::lookAt(cameraPos, cameraTarget, glm::vec3(0.0f, 1.0f, 0.0f));
        glm::mat4 model = glm::mat4(1.0f);
        model = glm::rotate(model, glm::radians(rotationAngle), glm::vec3(0.0f, 1.0f, 0.0f));
        
        // Render globe
        globeShader->use();
        globeShader->setMat4("projection", projection);
        globeShader->setMat4("view", view);
        globeShader->setMat4("model", model);
        globeShader->setVec3("lightPos", glm::vec3(5.0f, 5.0f, 5.0f));
        globeShader->setVec3("viewPos", cameraPos);
        globeShader->setVec3("objectColor", glm::vec3(0.3f, 0.5f, 0.8f));
        globeShader->setBool("useTexture", true);
        
        glBindVertexArray(sphereVAO);
        glDrawElements(GL_TRIANGLES, sphereIndices.size(), GL_UNSIGNED_INT, 0);
        
        // Render exchange markers (now with screen dimensions for hover detection)
        renderExchangeMarkers(exchanges, projection, view, model, width, height);
        
        // Render trade routes (now synchronized with globe rotation)
        renderTradeRoutesStatic(projection, view, model);
        
        glBindVertexArray(0);
        glDisable(GL_DEPTH_TEST);
    }
    
    /**
     * Camera controls
     */
    void zoomIn() { cameraDistance = std::max(1.5f, cameraDistance - 0.2f); updateCamera(); }
    void zoomOut() { cameraDistance = std::min(10.0f, cameraDistance + 0.2f); updateCamera(); }
    void resetCamera() { cameraDistance = 3.0f; rotationAngle = 0.0f; updateCamera(); }
    
    /**
     * Mouse interaction
     */
    void updateMousePosition(double x, double y, int width, int height) { 
        mouseX = x; 
        mouseY = y;
        
        // Update hovered exchange based on screen position
        updateHoveredExchange(width, height);
    }
    
    int getHoveredExchange() const { return hoveredExchangeIndex; }
    int getSelectedExchange() const { return selectedExchangeIndex; }
    
    void handleClick() {
        if (hoveredExchangeIndex >= 0) {
            if (selectedExchangeIndex == hoveredExchangeIndex) {
                selectedExchangeIndex = -1; // Deselect if clicking same
            } else {
                selectedExchangeIndex = hoveredExchangeIndex;
            }
            routesNeedUpdate = true; // Force route color update
        }
    }
    
    void clearSelection() { 
        selectedExchangeIndex = -1; 
        selectedRouteIndex = -1;
        routesNeedUpdate = true;
    }
    
private:
    void generateSphere(float radius, int sectors, int stacks) {
        sphereVertices.clear();
        sphereIndices.clear();
        
        float sectorStep = 2 * M_PI / sectors;
        float stackStep = M_PI / stacks;
        
        for (int i = 0; i <= stacks; ++i) {
            float stackAngle = M_PI / 2 - i * stackStep;
            float xy = radius * cosf(stackAngle);
            float z = radius * sinf(stackAngle);
            
            for (int j = 0; j <= sectors; ++j) {
                float sectorAngle = j * sectorStep;
                
                float x = xy * cosf(sectorAngle);
                float y = xy * sinf(sectorAngle);
                
                // Position
                sphereVertices.push_back(x);
                sphereVertices.push_back(z);
                sphereVertices.push_back(y);
                
                // Normal
                sphereVertices.push_back(x / radius);
                sphereVertices.push_back(z / radius);
                sphereVertices.push_back(y / radius);
                
                // TexCoord
                sphereVertices.push_back((float)j / sectors);
                sphereVertices.push_back((float)i / stacks);
            }
        }
        
        // Indices
        for (int i = 0; i < stacks; ++i) {
            int k1 = i * (sectors + 1);
            int k2 = k1 + sectors + 1;
            
            for (int j = 0; j < sectors; ++j, ++k1, ++k2) {
                if (i != 0) {
                    sphereIndices.push_back(k1);
                    sphereIndices.push_back(k2);
                    sphereIndices.push_back(k1 + 1);
                }
                
                if (i != (stacks - 1)) {
                    sphereIndices.push_back(k1 + 1);
                    sphereIndices.push_back(k2);
                    sphereIndices.push_back(k2 + 1);
                }
            }
        }
    }
    
    void renderExchangeMarkers(const std::vector<Exchange>& exchanges,
                               const glm::mat4& projection, const glm::mat4& view,
                               const glm::mat4& globeModel, int screenWidth, int screenHeight) {
        
        globeShader->use();
        globeShader->setBool("useTexture", false);
        
        hoveredExchangeIndex = -1; // Reset hover
        
        for (size_t idx = 0; idx < exchanges.size(); idx++) {
            const auto& ex = exchanges[idx];
            glm::vec3 pos = latLonToCartesian(ex.latitude, ex.longitude, 1.05f);
            
            // Transform position to screen space for hover detection
            glm::vec4 worldPos = globeModel * glm::vec4(pos, 1.0f);
            glm::vec4 clipPos = projection * view * worldPos;
            
            if (clipPos.w != 0.0f) {
                glm::vec3 ndcPos = glm::vec3(clipPos) / clipPos.w;
                
                // Convert NDC to screen coordinates
                float screenX = (ndcPos.x + 1.0f) * 0.5f * screenWidth;
                float screenY = (1.0f - ndcPos.y) * 0.5f * screenHeight;
                
                // Check if mouse is hovering
                if (clipPos.z > 0 && isMouseNear(screenX, screenY, 25.0f)) {
                    hoveredExchangeIndex = idx;
                }
            }
            
            glm::mat4 model = globeModel;
            model = glm::translate(model, pos);
            
            // Size based on state
            float scale = 0.02f;
            bool isSelected = ((int)idx == selectedExchangeIndex);
            bool isHovered = ((int)idx == hoveredExchangeIndex);
            
            if (isSelected) {
                scale = 0.04f; // Selected - largest
            } else if (isHovered) {
                scale = 0.03f; // Hovered - medium
            }
            
            model = glm::scale(model, glm::vec3(scale));
            
            globeShader->setMat4("model", model);
            
            // Color by state and type
            glm::vec3 color;
            if (isSelected) {
                color = glm::vec3(1.0f, 0.0f, 1.0f); // Magenta when selected
            } else if (isHovered) {
                color = glm::vec3(0.0f, 1.0f, 1.0f); // Cyan when hovered
            } else {
                // Normal colors by type
                switch (ex.type) {
                    case ExchangeType::EQUITY: color = glm::vec3(1.0f, 0.5f, 0.0f); break;
                    case ExchangeType::CRYPTO: color = glm::vec3(1.0f, 1.0f, 0.0f); break;
                    case ExchangeType::DERIVATIVES: color = glm::vec3(0.5f, 0.0f, 1.0f); break;
                    default: color = glm::vec3(1.0f, 1.0f, 1.0f);
                }
            }
            globeShader->setVec3("objectColor", color);
            
            glBindVertexArray(sphereVAO);
            glDrawElements(GL_TRIANGLES, sphereIndices.size(), GL_UNSIGNED_INT, 0);
        }
    }
    
    void renderTradeRoutes(const std::vector<ArbitrageOpportunity>& opportunities,
                          const glm::mat4& projection, const glm::mat4& view,
                          const glm::mat4& model) {
        
        if (opportunities.empty()) return;
        
        lineVertices.clear();
        
        // Generate arcs for top 10 opportunities
        int count = std::min(10, (int)opportunities.size());
        for (int i = 0; i < count; ++i) {
            const auto& opp = opportunities[i];
            
            // Find exchange positions (simplified - need actual lookup)
            // For now, generate random arcs
            float lat1 = (rand() % 180 - 90) * 1.0f;
            float lon1 = (rand() % 360 - 180) * 1.0f;
            float lat2 = (rand() % 180 - 90) * 1.0f;
            float lon2 = (rand() % 360 - 180) * 1.0f;
            
            glm::vec3 start = latLonToCartesian(lat1, lon1, 1.02f);
            glm::vec3 end = latLonToCartesian(lat2, lon2, 1.02f);
            
            // Generate arc
            int segments = 50;
            for (int j = 0; j < segments; ++j) {
                float t = (float)j / segments;
                glm::vec3 point = glm::mix(start, end, t);
                point = glm::normalize(point) * 1.02f;
                
                // Position
                lineVertices.push_back(point.x);
                lineVertices.push_back(point.y);
                lineVertices.push_back(point.z);
                
                // Color (green for profitable)
                float intensity = 1.0f - t * 0.5f;
                lineVertices.push_back(0.0f);
                lineVertices.push_back(intensity);
                lineVertices.push_back(0.0f);
            }
        }
        
        if (!lineVertices.empty()) {
            lineShader->use();
            lineShader->setMat4("projection", projection);
            lineShader->setMat4("view", view);
            lineShader->setMat4("model", model);
            
            glBindVertexArray(lineVAO);
            glBindBuffer(GL_ARRAY_BUFFER, lineVBO);
            glBufferData(GL_ARRAY_BUFFER, lineVertices.size() * sizeof(float),
                        lineVertices.data(), GL_DYNAMIC_DRAW);
            
            glLineWidth(2.0f);
            glDrawArrays(GL_LINE_STRIP, 0, lineVertices.size() / 6);
        }
    }
    
    /**
     * Generate trade routes from opportunities (called once when opportunities change)
     */
    void generateTradeRoutes(const std::vector<Exchange>& exchanges,
                            const std::vector<ArbitrageOpportunity>& opportunities) {
        lineVertices.clear();
        
        if (opportunities.empty()) return;
        
        // Create a map for fast exchange lookup
        std::map<std::string, const Exchange*> exchangeMap;
        for (const auto& ex : exchanges) {
            exchangeMap[ex.id] = &ex;
        }
        
        // Generate arcs for top 10 opportunities
        int count = std::min(10, (int)opportunities.size());
        for (int i = 0; i < count; ++i) {
            const auto& opp = opportunities[i];
            
            // Look up actual exchange positions
            auto buyEx = exchangeMap.find(opp.buy_exchange);
            auto sellEx = exchangeMap.find(opp.sell_exchange);
            
            if (buyEx == exchangeMap.end() || sellEx == exchangeMap.end()) {
                continue;
            }
            
            glm::vec3 start = latLonToCartesian(buyEx->second->latitude, buyEx->second->longitude, 1.02f);
            glm::vec3 end = latLonToCartesian(sellEx->second->latitude, sellEx->second->longitude, 1.02f);
            
            // Generate smooth arc between exchanges
            int segments = 50;
            for (int j = 0; j < segments; ++j) {
                float t = (float)j / segments;
                
                // Interpolate on sphere surface using simple lerp + normalize
                glm::vec3 point = glm::normalize(glm::mix(start, end, t));
                
                // Add height to arc (makes it more visible)
                float arcHeight = std::sin(t * 3.14159f) * 0.2f;
                point = point * (1.02f + arcHeight);
                
                // Position
                lineVertices.push_back(point.x);
                lineVertices.push_back(point.y);
                lineVertices.push_back(point.z);
                
                // Color - fade from bright green to darker green
                // If this route involves selected exchange, make it brighter
                bool isSelectedRoute = false;
                if (selectedExchangeIndex >= 0 && selectedExchangeIndex < (int)exchanges.size()) {
                    const std::string& selId = exchanges[selectedExchangeIndex].id;
                    if (opp.buy_exchange == selId || opp.sell_exchange == selId) {
                        isSelectedRoute = true;
                    }
                }
                
                float intensity = 0.5f + 0.5f * (1.0f - t);
                if (isSelectedRoute) {
                    // Yellow/bright for selected routes
                    lineVertices.push_back(intensity);
                    lineVertices.push_back(intensity);
                    lineVertices.push_back(0.0f);
                } else {
                    // Green for normal routes
                    lineVertices.push_back(0.0f);
                    lineVertices.push_back(intensity);
                    lineVertices.push_back(0.1f);
                }
            }
        }
        
        // Upload to GPU
        if (!lineVertices.empty()) {
            glBindVertexArray(lineVAO);
            glBindBuffer(GL_ARRAY_BUFFER, lineVBO);
            glBufferData(GL_ARRAY_BUFFER, lineVertices.size() * sizeof(float),
                        lineVertices.data(), GL_STATIC_DRAW);
            glBindVertexArray(0);
        }
    }
    
    /**
     * Render pre-generated trade routes (synchronized with globe)
     */
    void renderTradeRoutesStatic(const glm::mat4& projection, const glm::mat4& view,
                                 const glm::mat4& model) {
        if (lineVertices.empty()) return;
        
        lineShader->use();
        lineShader->setMat4("projection", projection);
        lineShader->setMat4("view", view);
        lineShader->setMat4("model", model);
        lineShader->setFloat("alpha", routeAlpha); // Apply fade-in
        
        glBindVertexArray(lineVAO);
        glLineWidth(2.5f);
        
        // Draw each route separately
        int segments = 50;
        int numRoutes = lineVertices.size() / (segments * 6);
        
        for (int i = 0; i < numRoutes; i++) {
            glDrawArrays(GL_LINE_STRIP, i * segments, segments);
        }
        
        glBindVertexArray(0);
    }
    
    void updateCamera() {
        cameraPos = glm::vec3(0.0f, 0.0f, cameraDistance);
    }
    
    /**
     * Update which exchange the mouse is hovering over
     */
    void updateHoveredExchange(int screenWidth, int screenHeight) {
        hoveredExchangeIndex = -1;
        
        // Simple 2D distance check (approximation)
        // In production, you'd do proper 3D ray-sphere intersection
        
        // This is called from render, so we already have the exchanges
        // We'll do the actual check in renderExchangeMarkers
    }
    
    /**
     * Check if mouse is near a screen position
     */
    bool isMouseNear(float screenX, float screenY, float threshold = 20.0f) const {
        float dx = mouseX - screenX;
        float dy = mouseY - screenY;
        return (dx * dx + dy * dy) < (threshold * threshold);
    }
    
    void cleanup() {
        if (sphereVAO) glDeleteVertexArrays(1, &sphereVAO);
        if (sphereVBO) glDeleteBuffers(1, &sphereVBO);
        if (sphereEBO) glDeleteBuffers(1, &sphereEBO);
        if (markerVAO) glDeleteVertexArrays(1, &markerVAO);
        if (markerVBO) glDeleteBuffers(1, &markerVBO);
        if (lineVAO) glDeleteVertexArrays(1, &lineVAO);
        if (lineVBO) glDeleteBuffers(1, &lineVBO);
        
        delete globeShader;
        delete lineShader;
    }
};