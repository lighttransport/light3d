#include "light3d/light3d.h"
#include <iostream>

namespace light3d {

// OpenGL Renderer Implementation
class RendererOpenGL : public Renderer {
public:
    RendererOpenGL() = default;
    ~RendererOpenGL() override = default;
    
    bool initialize(uint32_t width, uint32_t height) override {
        width_ = width;
        height_ = height;
        std::cout << "OpenGL Renderer initialized: " << width << "x" << height << std::endl;
        return true;
    }
    
    void shutdown() override {
        std::cout << "OpenGL Renderer shutdown" << std::endl;
    }
    
    void resize(uint32_t width, uint32_t height) override {
        width_ = width;
        height_ = height;
    }
    
    void clear(const Color& color) override {
        // Placeholder for glClear implementation
    }
    
    void present() override {
        // Placeholder for swap buffers implementation
    }
    
    RenderBackend getBackend() const override {
        return RenderBackend::OpenGL;
    }
    
private:
    uint32_t width_ = 0;
    uint32_t height_ = 0;
};

// WebGL Renderer stub
#ifdef LIGHT3D_PLATFORM_WASM
class RendererWebGL : public Renderer {
public:
    RendererWebGL() = default;
    ~RendererWebGL() override = default;
    
    bool initialize(uint32_t width, uint32_t height) override {
        width_ = width;
        height_ = height;
        std::cout << "WebGL Renderer initialized: " << width << "x" << height << std::endl;
        return true;
    }
    
    void shutdown() override {
        std::cout << "WebGL Renderer shutdown" << std::endl;
    }
    
    void resize(uint32_t width, uint32_t height) override {
        width_ = width;
        height_ = height;
    }
    
    void clear(const Color& color) override {
        // Placeholder for WebGL clear implementation
    }
    
    void present() override {
        // Placeholder for WebGL present implementation
    }
    
    RenderBackend getBackend() const override {
        return RenderBackend::WebGL;
    }
    
private:
    uint32_t width_ = 0;
    uint32_t height_ = 0;
};
#endif

// Engine Implementation
class EngineImpl : public Engine {
public:
    EngineImpl(RenderBackend backend) : backend_(backend), running_(false) {}
    ~EngineImpl() override = default;
    
    bool initialize(const EngineConfig& config) override {
        std::cout << "Light3D Engine v" << getVersionString() << std::endl;
        std::cout << "Initializing with backend: " << getRenderBackendName(config.backend) << std::endl;
        
        // Create renderer based on backend
        switch(config.backend) {
            case RenderBackend::OpenGL:
#ifdef LIGHT3D_OPENGL_ENABLED
                renderer_ = std::make_unique<RendererOpenGL>();
#else
                std::cerr << "OpenGL backend not enabled" << std::endl;
                return false;
#endif
                break;
                
            case RenderBackend::WebGL:
#ifdef LIGHT3D_PLATFORM_WASM
                renderer_ = std::make_unique<RendererWebGL>();
#else
                std::cerr << "WebGL backend only available on WASM platform" << std::endl;
                return false;
#endif
                break;
                
            default:
                std::cerr << "Unsupported backend" << std::endl;
                return false;
        }
        
        if (!renderer_->initialize(config.width, config.height)) {
            std::cerr << "Failed to initialize renderer" << std::endl;
            return false;
        }
        
        running_ = true;
        scene_ = std::make_unique<Scene>();
        camera_ = std::make_unique<Camera>();
        
        return true;
    }
    
    void shutdown() override {
        if (renderer_) {
            renderer_->shutdown();
        }
        running_ = false;
        std::cout << "Engine shutdown complete" << std::endl;
    }
    
    bool isRunning() const override {
        return running_;
    }
    
    void update(float deltaTime) override {
        // Update logic here
    }
    
    void render() override {
        if (renderer_ && scene_) {
            renderer_->clear(scene_->getBackgroundColor());
            // Render scene here
            renderer_->present();
        }
    }
    
    Renderer* getRenderer() override {
        return renderer_.get();
    }
    
private:
    RenderBackend backend_;
    bool running_;
    std::unique_ptr<Renderer> renderer_;
    std::unique_ptr<Scene> scene_;
    std::unique_ptr<Camera> camera_;
};

// Factory function
std::unique_ptr<Engine> Engine::create(RenderBackend backend) {
    return std::make_unique<EngineImpl>(backend);
}

} // namespace light3d
