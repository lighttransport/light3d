#pragma once
// Vulkan rendering backend for glview.
// Uses volk for dynamic loading (no Vulkan SDK required at build time).

#include <volk.h>
#include <GLFW/glfw3.h>
#include <vector>
#include <cstdint>

struct VkMesh {
    VkBuffer buffer = VK_NULL_HANDLE;
    VkDeviceMemory memory = VK_NULL_HANDLE;
    int vertexCount = 0;
};

struct VulkanBackend {
    // Initialise Vulkan: instance, device, swapchain, pipelines.
    // Returns false on failure.
    bool init(GLFWwindow* window);

    // Upload vertex data (11 floats/vertex: pos3+color3+normal3+uv2).
    VkMesh createMesh(const std::vector<float>& verts);
    void destroyMesh(VkMesh& mesh);

    // Begin a frame: acquire swapchain image, begin command buffer.
    // Returns false if swapchain is out of date (caller should skip frame).
    bool beginFrame();

    // Draw a mesh with given model and viewProj matrices.
    // topology: VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST or LINE_LIST.
    void draw(const VkMesh& mesh, const float model[16], const float viewProj[16],
              VkPrimitiveTopology topology);

    // End frame: end command buffer, submit, present.
    void endFrame();

    // Cleanup everything.
    void cleanup();

    // Handle resize (called from GLFW framebuffer size callback).
    void onResize(int width, int height);

    // Public state
    bool framebufferResized = false;

private:
    GLFWwindow* window_ = nullptr;

    VkInstance instance_ = VK_NULL_HANDLE;
    VkSurfaceKHR surface_ = VK_NULL_HANDLE;
    VkPhysicalDevice physicalDevice_ = VK_NULL_HANDLE;
    VkDevice device_ = VK_NULL_HANDLE;
    VkQueue graphicsQueue_ = VK_NULL_HANDLE;
    VkQueue presentQueue_ = VK_NULL_HANDLE;
    uint32_t graphicsFamily_ = 0;
    uint32_t presentFamily_ = 0;

    // Swapchain
    VkSwapchainKHR swapchain_ = VK_NULL_HANDLE;
    VkFormat swapchainFormat_ = VK_FORMAT_UNDEFINED;
    VkExtent2D swapchainExtent_ = {};
    std::vector<VkImage> swapchainImages_;
    std::vector<VkImageView> swapchainImageViews_;

    // Depth buffer
    VkImage depthImage_ = VK_NULL_HANDLE;
    VkDeviceMemory depthMemory_ = VK_NULL_HANDLE;
    VkImageView depthImageView_ = VK_NULL_HANDLE;

    // Render pass & framebuffers
    VkRenderPass renderPass_ = VK_NULL_HANDLE;
    std::vector<VkFramebuffer> framebuffers_;

    // Pipelines
    VkPipelineLayout pipelineLayout_ = VK_NULL_HANDLE;
    VkPipeline trianglePipeline_ = VK_NULL_HANDLE;
    VkPipeline linePipeline_ = VK_NULL_HANDLE;

    // Command pool & buffers (2 frames in flight)
    static const int kMaxFramesInFlight = 2;
    VkCommandPool commandPool_ = VK_NULL_HANDLE;
    VkCommandBuffer commandBuffers_[kMaxFramesInFlight] = {};

    // Sync
    VkSemaphore imageAvailableSems_[kMaxFramesInFlight] = {};
    VkSemaphore renderFinishedSems_[kMaxFramesInFlight] = {};
    VkFence inFlightFences_[kMaxFramesInFlight] = {};
    uint32_t currentFrame_ = 0;
    uint32_t imageIndex_ = 0;

    // Helpers
    bool createInstance();
    bool pickPhysicalDevice();
    bool createLogicalDevice();
    bool createSwapchain();
    bool createDepthResources();
    bool createRenderPass();
    bool createFramebuffers();
    bool createPipelineLayout();
    bool createPipelines();
    bool createCommandPool();
    bool createCommandBuffers();
    bool createSyncObjects();
    void cleanupSwapchain();
    void recreateSwapchain();

    uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties);
    VkShaderModule createShaderModule(const uint32_t* code, size_t sizeBytes);
    VkPipeline createGraphicsPipeline(VkPrimitiveTopology topology);
};
