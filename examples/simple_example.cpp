#include "light3d/light3d.h"
#include <iostream>
#include <chrono>
#include <thread>

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#include <emscripten/html5.h>

// Global engine instance for WASM main loop
light3d::Engine* g_engine = nullptr;

void main_loop() {
    static auto lastTime = std::chrono::high_resolution_clock::now();
    auto currentTime = std::chrono::high_resolution_clock::now();
    float deltaTime = std::chrono::duration<float>(currentTime - lastTime).count();
    lastTime = currentTime;
    
    if (g_engine && g_engine->isRunning()) {
        g_engine->update(deltaTime);
        g_engine->render();
    }
}
#endif

int main(int argc, char* argv[]) {
    std::cout << "Light3D Simple Example" << std::endl;
    
    // Create engine configuration
    light3d::EngineConfig config;
    config.width = 800;
    config.height = 600;
    config.title = "Light3D - Simple Example";
    
#ifdef __EMSCRIPTEN__
    // Use WebGL for WASM builds
    config.backend = light3d::RenderBackend::WebGL;
#else
    // Use OpenGL for native builds
    config.backend = light3d::RenderBackend::OpenGL;
#endif
    
    // Create engine
    auto engine = light3d::Engine::create(config.backend);
    if (!engine) {
        std::cerr << "Failed to create engine" << std::endl;
        return 1;
    }
    
    // Initialize engine
    if (!engine->initialize(config)) {
        std::cerr << "Failed to initialize engine" << std::endl;
        return 1;
    }
    
    std::cout << "Engine initialized successfully" << std::endl;
    std::cout << "Backend: " << light3d::getRenderBackendName(config.backend) << std::endl;
    
#ifdef __EMSCRIPTEN__
    // WASM: Use emscripten main loop
    g_engine = engine.get();
    emscripten_set_main_loop(main_loop, 0, 1);
#else
    // Native: Simple loop for demonstration
    std::cout << "Running for 5 seconds..." << std::endl;
    
    auto startTime = std::chrono::high_resolution_clock::now();
    auto lastTime = startTime;
    int frameCount = 0;
    
    while (engine->isRunning()) {
        auto currentTime = std::chrono::high_resolution_clock::now();
        float deltaTime = std::chrono::duration<float>(currentTime - lastTime).count();
        float elapsedTime = std::chrono::duration<float>(currentTime - startTime).count();
        lastTime = currentTime;
        
        engine->update(deltaTime);
        engine->render();
        
        frameCount++;
        
        // Run for 5 seconds then exit
        if (elapsedTime > 5.0f) {
            break;
        }
        
        // Simple frame rate limiting
        std::this_thread::sleep_for(std::chrono::milliseconds(16)); // ~60 FPS
    }
    
    std::cout << "Rendered " << frameCount << " frames in 5 seconds" << std::endl;
    std::cout << "Average FPS: " << (frameCount / 5.0f) << std::endl;
    
    engine->shutdown();
#endif
    
    return 0;
}
