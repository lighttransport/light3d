#include "vk_backend.h"
#include "vk_shaders.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <limits>

// Push constants: model(mat4) + viewProj(mat4) = 128 bytes
struct PushConstants {
    float model[16];
    float viewProj[16];
};

// ---------------------------------------------------------------------------
// Init / cleanup
// ---------------------------------------------------------------------------

bool VulkanBackend::init(GLFWwindow* window) {
    window_ = window;

    if (volkInitialize() != VK_SUCCESS) {
        std::fprintf(stderr, "Vulkan: volk failed to find a Vulkan loader\n");
        return false;
    }

    if (!createInstance()) return false;

    volkLoadInstance(instance_);

    if (glfwCreateWindowSurface(instance_, window_, nullptr, &surface_) != VK_SUCCESS) {
        std::fprintf(stderr, "Vulkan: failed to create window surface\n");
        return false;
    }

    if (!pickPhysicalDevice()) return false;
    if (!createLogicalDevice()) return false;

    volkLoadDevice(device_);

    if (!createSwapchain()) return false;
    if (!createDepthResources()) return false;
    if (!createRenderPass()) return false;
    if (!createFramebuffers()) return false;
    if (!createPipelineLayout()) return false;
    if (!createPipelines()) return false;
    if (!createCommandPool()) return false;
    if (!createCommandBuffers()) return false;
    if (!createSyncObjects()) return false;

    std::printf("Vulkan backend initialised successfully\n");
    return true;
}

void VulkanBackend::cleanup() {
    if (device_ == VK_NULL_HANDLE) return;
    vkDeviceWaitIdle(device_);

    cleanupSwapchain();

    if (trianglePipeline_) vkDestroyPipeline(device_, trianglePipeline_, nullptr);
    if (linePipeline_) vkDestroyPipeline(device_, linePipeline_, nullptr);
    if (pipelineLayout_) vkDestroyPipelineLayout(device_, pipelineLayout_, nullptr);
    if (renderPass_) vkDestroyRenderPass(device_, renderPass_, nullptr);

    for (int i = 0; i < kMaxFramesInFlight; ++i) {
        if (imageAvailableSems_[i]) vkDestroySemaphore(device_, imageAvailableSems_[i], nullptr);
        if (renderFinishedSems_[i]) vkDestroySemaphore(device_, renderFinishedSems_[i], nullptr);
        if (inFlightFences_[i]) vkDestroyFence(device_, inFlightFences_[i], nullptr);
    }

    if (commandPool_) vkDestroyCommandPool(device_, commandPool_, nullptr);
    if (device_) vkDestroyDevice(device_, nullptr);
    if (surface_) vkDestroySurfaceKHR(instance_, surface_, nullptr);
    if (instance_) vkDestroyInstance(instance_, nullptr);

    volkFinalize();
    device_ = VK_NULL_HANDLE;
}

// ---------------------------------------------------------------------------
// Instance
// ---------------------------------------------------------------------------

bool VulkanBackend::createInstance() {
    VkApplicationInfo appInfo = {};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = "glview";
    appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.pEngineName = "light3d";
    appInfo.engineVersion = VK_MAKE_VERSION(0, 1, 0);
    appInfo.apiVersion = VK_API_VERSION_1_0;

    uint32_t glfwExtCount = 0;
    const char** glfwExts = glfwGetRequiredInstanceExtensions(&glfwExtCount);

    VkInstanceCreateInfo createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createInfo.pApplicationInfo = &appInfo;
    createInfo.enabledExtensionCount = glfwExtCount;
    createInfo.ppEnabledExtensionNames = glfwExts;

    VkResult result = vkCreateInstance(&createInfo, nullptr, &instance_);
    if (result != VK_SUCCESS) {
        std::fprintf(stderr, "Vulkan: vkCreateInstance failed (%d)\n", static_cast<int>(result));
        return false;
    }
    return true;
}

// ---------------------------------------------------------------------------
// Physical device
// ---------------------------------------------------------------------------

bool VulkanBackend::pickPhysicalDevice() {
    uint32_t count = 0;
    vkEnumeratePhysicalDevices(instance_, &count, nullptr);
    if (count == 0) {
        std::fprintf(stderr, "Vulkan: no GPUs with Vulkan support\n");
        return false;
    }

    std::vector<VkPhysicalDevice> devices(count);
    vkEnumeratePhysicalDevices(instance_, &count, devices.data());

    for (auto& dev : devices) {
        // Find queue families
        uint32_t queueFamilyCount = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(dev, &queueFamilyCount, nullptr);
        std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
        vkGetPhysicalDeviceQueueFamilyProperties(dev, &queueFamilyCount, queueFamilies.data());

        bool foundGraphics = false, foundPresent = false;
        for (uint32_t i = 0; i < queueFamilyCount; ++i) {
            if (queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
                graphicsFamily_ = i;
                foundGraphics = true;
            }
            VkBool32 presentSupport = false;
            vkGetPhysicalDeviceSurfaceSupportKHR(dev, i, surface_, &presentSupport);
            if (presentSupport) {
                presentFamily_ = i;
                foundPresent = true;
            }
            if (foundGraphics && foundPresent) break;
        }

        if (!foundGraphics || !foundPresent) continue;

        // Check swapchain extension
        uint32_t extCount = 0;
        vkEnumerateDeviceExtensionProperties(dev, nullptr, &extCount, nullptr);
        std::vector<VkExtensionProperties> exts(extCount);
        vkEnumerateDeviceExtensionProperties(dev, nullptr, &extCount, exts.data());

        bool hasSwapchain = false;
        for (auto& ext : exts) {
            if (std::strcmp(ext.extensionName, VK_KHR_SWAPCHAIN_EXTENSION_NAME) == 0) {
                hasSwapchain = true;
                break;
            }
        }
        if (!hasSwapchain) continue;

        physicalDevice_ = dev;

        VkPhysicalDeviceProperties props;
        vkGetPhysicalDeviceProperties(dev, &props);
        std::printf("Vulkan GPU: %s\n", props.deviceName);
        return true;
    }

    std::fprintf(stderr, "Vulkan: no suitable GPU found\n");
    return false;
}

// ---------------------------------------------------------------------------
// Logical device
// ---------------------------------------------------------------------------

bool VulkanBackend::createLogicalDevice() {
    float queuePriority = 1.0f;

    std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
    // Avoid duplicate if same family
    std::vector<uint32_t> uniqueFamilies = {graphicsFamily_};
    if (presentFamily_ != graphicsFamily_)
        uniqueFamilies.push_back(presentFamily_);

    for (uint32_t family : uniqueFamilies) {
        VkDeviceQueueCreateInfo qci = {};
        qci.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        qci.queueFamilyIndex = family;
        qci.queueCount = 1;
        qci.pQueuePriorities = &queuePriority;
        queueCreateInfos.push_back(qci);
    }

    VkPhysicalDeviceFeatures deviceFeatures = {};
    deviceFeatures.fillModeNonSolid = VK_TRUE;  // for wireframe if needed later
    deviceFeatures.wideLines = VK_FALSE;

    const char* extensions[] = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};

    VkDeviceCreateInfo createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    createInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
    createInfo.pQueueCreateInfos = queueCreateInfos.data();
    createInfo.pEnabledFeatures = &deviceFeatures;
    createInfo.enabledExtensionCount = 1;
    createInfo.ppEnabledExtensionNames = extensions;

    if (vkCreateDevice(physicalDevice_, &createInfo, nullptr, &device_) != VK_SUCCESS) {
        std::fprintf(stderr, "Vulkan: failed to create logical device\n");
        return false;
    }

    vkGetDeviceQueue(device_, graphicsFamily_, 0, &graphicsQueue_);
    vkGetDeviceQueue(device_, presentFamily_, 0, &presentQueue_);
    return true;
}

// ---------------------------------------------------------------------------
// Swapchain
// ---------------------------------------------------------------------------

bool VulkanBackend::createSwapchain() {
    VkSurfaceCapabilitiesKHR caps;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice_, surface_, &caps);

    uint32_t formatCount = 0;
    vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice_, surface_, &formatCount, nullptr);
    std::vector<VkSurfaceFormatKHR> formats(formatCount);
    vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice_, surface_, &formatCount, formats.data());

    uint32_t presentModeCount = 0;
    vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice_, surface_, &presentModeCount, nullptr);
    std::vector<VkPresentModeKHR> presentModes(presentModeCount);
    vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice_, surface_, &presentModeCount, presentModes.data());

    // Choose format: prefer SRGB B8G8R8A8
    VkSurfaceFormatKHR surfaceFormat = formats[0];
    for (auto& f : formats) {
        if (f.format == VK_FORMAT_B8G8R8A8_SRGB && f.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            surfaceFormat = f;
            break;
        }
    }
    // Fallback: try UNORM
    if (surfaceFormat.format != VK_FORMAT_B8G8R8A8_SRGB) {
        for (auto& f : formats) {
            if (f.format == VK_FORMAT_B8G8R8A8_UNORM) {
                surfaceFormat = f;
                break;
            }
        }
    }
    swapchainFormat_ = surfaceFormat.format;

    // Choose present mode: prefer FIFO (vsync)
    VkPresentModeKHR presentMode = VK_PRESENT_MODE_FIFO_KHR;

    // Choose extent
    if (caps.currentExtent.width != 0xFFFFFFFF) {
        swapchainExtent_ = caps.currentExtent;
    } else {
        int w, h;
        glfwGetFramebufferSize(window_, &w, &h);
        swapchainExtent_.width = std::max(caps.minImageExtent.width,
            std::min(caps.maxImageExtent.width, static_cast<uint32_t>(w)));
        swapchainExtent_.height = std::max(caps.minImageExtent.height,
            std::min(caps.maxImageExtent.height, static_cast<uint32_t>(h)));
    }

    uint32_t imageCount = caps.minImageCount + 1;
    if (caps.maxImageCount > 0 && imageCount > caps.maxImageCount)
        imageCount = caps.maxImageCount;

    VkSwapchainCreateInfoKHR sci = {};
    sci.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    sci.surface = surface_;
    sci.minImageCount = imageCount;
    sci.imageFormat = surfaceFormat.format;
    sci.imageColorSpace = surfaceFormat.colorSpace;
    sci.imageExtent = swapchainExtent_;
    sci.imageArrayLayers = 1;
    sci.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

    uint32_t queueFamilyIndices[] = {graphicsFamily_, presentFamily_};
    if (graphicsFamily_ != presentFamily_) {
        sci.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        sci.queueFamilyIndexCount = 2;
        sci.pQueueFamilyIndices = queueFamilyIndices;
    } else {
        sci.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    }

    sci.preTransform = caps.currentTransform;
    sci.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    sci.presentMode = presentMode;
    sci.clipped = VK_TRUE;

    if (vkCreateSwapchainKHR(device_, &sci, nullptr, &swapchain_) != VK_SUCCESS) {
        std::fprintf(stderr, "Vulkan: failed to create swapchain\n");
        return false;
    }

    // Get images
    vkGetSwapchainImagesKHR(device_, swapchain_, &imageCount, nullptr);
    swapchainImages_.resize(imageCount);
    vkGetSwapchainImagesKHR(device_, swapchain_, &imageCount, swapchainImages_.data());

    // Create image views
    swapchainImageViews_.resize(imageCount);
    for (uint32_t i = 0; i < imageCount; ++i) {
        VkImageViewCreateInfo ivci = {};
        ivci.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        ivci.image = swapchainImages_[i];
        ivci.viewType = VK_IMAGE_VIEW_TYPE_2D;
        ivci.format = swapchainFormat_;
        ivci.components = {VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY,
                           VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY};
        ivci.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        ivci.subresourceRange.levelCount = 1;
        ivci.subresourceRange.layerCount = 1;

        if (vkCreateImageView(device_, &ivci, nullptr, &swapchainImageViews_[i]) != VK_SUCCESS) {
            std::fprintf(stderr, "Vulkan: failed to create image view\n");
            return false;
        }
    }

    return true;
}

// ---------------------------------------------------------------------------
// Depth buffer
// ---------------------------------------------------------------------------

bool VulkanBackend::createDepthResources() {
    VkFormat depthFormat = VK_FORMAT_D32_SFLOAT;

    VkImageCreateInfo ici = {};
    ici.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    ici.imageType = VK_IMAGE_TYPE_2D;
    ici.format = depthFormat;
    ici.extent = {swapchainExtent_.width, swapchainExtent_.height, 1};
    ici.mipLevels = 1;
    ici.arrayLayers = 1;
    ici.samples = VK_SAMPLE_COUNT_1_BIT;
    ici.tiling = VK_IMAGE_TILING_OPTIMAL;
    ici.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
    ici.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    ici.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    if (vkCreateImage(device_, &ici, nullptr, &depthImage_) != VK_SUCCESS) {
        std::fprintf(stderr, "Vulkan: failed to create depth image\n");
        return false;
    }

    VkMemoryRequirements memReqs;
    vkGetImageMemoryRequirements(device_, depthImage_, &memReqs);

    VkMemoryAllocateInfo mai = {};
    mai.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    mai.allocationSize = memReqs.size;
    mai.memoryTypeIndex = findMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    if (vkAllocateMemory(device_, &mai, nullptr, &depthMemory_) != VK_SUCCESS) {
        std::fprintf(stderr, "Vulkan: failed to allocate depth image memory\n");
        return false;
    }
    vkBindImageMemory(device_, depthImage_, depthMemory_, 0);

    VkImageViewCreateInfo ivci = {};
    ivci.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    ivci.image = depthImage_;
    ivci.viewType = VK_IMAGE_VIEW_TYPE_2D;
    ivci.format = depthFormat;
    ivci.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
    ivci.subresourceRange.levelCount = 1;
    ivci.subresourceRange.layerCount = 1;

    if (vkCreateImageView(device_, &ivci, nullptr, &depthImageView_) != VK_SUCCESS) {
        std::fprintf(stderr, "Vulkan: failed to create depth image view\n");
        return false;
    }

    return true;
}

// ---------------------------------------------------------------------------
// Render pass
// ---------------------------------------------------------------------------

bool VulkanBackend::createRenderPass() {
    VkAttachmentDescription colorAttachment = {};
    colorAttachment.format = swapchainFormat_;
    colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    VkAttachmentDescription depthAttachment = {};
    depthAttachment.format = VK_FORMAT_D32_SFLOAT;
    depthAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    depthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkAttachmentReference colorRef = {};
    colorRef.attachment = 0;
    colorRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkAttachmentReference depthRef = {};
    depthRef.attachment = 1;
    depthRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass = {};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &colorRef;
    subpass.pDepthStencilAttachment = &depthRef;

    VkSubpassDependency dep = {};
    dep.srcSubpass = VK_SUBPASS_EXTERNAL;
    dep.dstSubpass = 0;
    dep.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
                       VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dep.srcAccessMask = 0;
    dep.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
                       VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dep.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT |
                        VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

    VkAttachmentDescription attachments[] = {colorAttachment, depthAttachment};

    VkRenderPassCreateInfo rpci = {};
    rpci.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    rpci.attachmentCount = 2;
    rpci.pAttachments = attachments;
    rpci.subpassCount = 1;
    rpci.pSubpasses = &subpass;
    rpci.dependencyCount = 1;
    rpci.pDependencies = &dep;

    if (vkCreateRenderPass(device_, &rpci, nullptr, &renderPass_) != VK_SUCCESS) {
        std::fprintf(stderr, "Vulkan: failed to create render pass\n");
        return false;
    }
    return true;
}

// ---------------------------------------------------------------------------
// Framebuffers
// ---------------------------------------------------------------------------

bool VulkanBackend::createFramebuffers() {
    framebuffers_.resize(swapchainImageViews_.size());
    for (size_t i = 0; i < swapchainImageViews_.size(); ++i) {
        VkImageView attachments[] = {swapchainImageViews_[i], depthImageView_};

        VkFramebufferCreateInfo fbci = {};
        fbci.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        fbci.renderPass = renderPass_;
        fbci.attachmentCount = 2;
        fbci.pAttachments = attachments;
        fbci.width = swapchainExtent_.width;
        fbci.height = swapchainExtent_.height;
        fbci.layers = 1;

        if (vkCreateFramebuffer(device_, &fbci, nullptr, &framebuffers_[i]) != VK_SUCCESS) {
            std::fprintf(stderr, "Vulkan: failed to create framebuffer\n");
            return false;
        }
    }
    return true;
}

// ---------------------------------------------------------------------------
// Pipeline layout (push constants)
// ---------------------------------------------------------------------------

bool VulkanBackend::createPipelineLayout() {
    VkPushConstantRange pushRange = {};
    pushRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    pushRange.offset = 0;
    pushRange.size = sizeof(PushConstants);

    VkPipelineLayoutCreateInfo plci = {};
    plci.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    plci.pushConstantRangeCount = 1;
    plci.pPushConstantRanges = &pushRange;

    if (vkCreatePipelineLayout(device_, &plci, nullptr, &pipelineLayout_) != VK_SUCCESS) {
        std::fprintf(stderr, "Vulkan: failed to create pipeline layout\n");
        return false;
    }
    return true;
}

// ---------------------------------------------------------------------------
// Pipelines
// ---------------------------------------------------------------------------

VkShaderModule VulkanBackend::createShaderModule(const uint32_t* code, size_t sizeBytes) {
    VkShaderModuleCreateInfo smci = {};
    smci.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    smci.codeSize = sizeBytes;
    smci.pCode = code;

    VkShaderModule module;
    if (vkCreateShaderModule(device_, &smci, nullptr, &module) != VK_SUCCESS) {
        std::fprintf(stderr, "Vulkan: failed to create shader module\n");
        return VK_NULL_HANDLE;
    }
    return module;
}

VkPipeline VulkanBackend::createGraphicsPipeline(VkPrimitiveTopology topology) {
    VkShaderModule vertModule = createShaderModule(kVkSimpleVert, sizeof(kVkSimpleVert));
    VkShaderModule fragModule = createShaderModule(kVkSimpleFrag, sizeof(kVkSimpleFrag));

    VkPipelineShaderStageCreateInfo stages[2] = {};
    stages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
    stages[0].module = vertModule;
    stages[0].pName = "main";
    stages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    stages[1].module = fragModule;
    stages[1].pName = "main";

    // Vertex input: 11 floats per vertex (pos3+color3+normal3+uv2)
    VkVertexInputBindingDescription binding = {};
    binding.binding = 0;
    binding.stride = 11 * sizeof(float);
    binding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    VkVertexInputAttributeDescription attrs[4] = {};
    // pos
    attrs[0].binding = 0;
    attrs[0].location = 0;
    attrs[0].format = VK_FORMAT_R32G32B32_SFLOAT;
    attrs[0].offset = 0;
    // color
    attrs[1].binding = 0;
    attrs[1].location = 1;
    attrs[1].format = VK_FORMAT_R32G32B32_SFLOAT;
    attrs[1].offset = 3 * sizeof(float);
    // normal
    attrs[2].binding = 0;
    attrs[2].location = 2;
    attrs[2].format = VK_FORMAT_R32G32B32_SFLOAT;
    attrs[2].offset = 6 * sizeof(float);
    // uv
    attrs[3].binding = 0;
    attrs[3].location = 3;
    attrs[3].format = VK_FORMAT_R32G32_SFLOAT;
    attrs[3].offset = 9 * sizeof(float);

    VkPipelineVertexInputStateCreateInfo vertexInput = {};
    vertexInput.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInput.vertexBindingDescriptionCount = 1;
    vertexInput.pVertexBindingDescriptions = &binding;
    vertexInput.vertexAttributeDescriptionCount = 4;
    vertexInput.pVertexAttributeDescriptions = attrs;

    VkPipelineInputAssemblyStateCreateInfo inputAssembly = {};
    inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssembly.topology = topology;

    VkPipelineViewportStateCreateInfo viewportState = {};
    viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.viewportCount = 1;
    viewportState.scissorCount = 1;

    VkPipelineRasterizationStateCreateInfo rasterizer = {};
    rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
    rasterizer.lineWidth = 1.0f;
    rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
    rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;

    // For line topology, disable backface culling
    if (topology == VK_PRIMITIVE_TOPOLOGY_LINE_LIST) {
        rasterizer.cullMode = VK_CULL_MODE_NONE;
    }

    VkPipelineMultisampleStateCreateInfo multisampling = {};
    multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineDepthStencilStateCreateInfo depthStencil = {};
    depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depthStencil.depthTestEnable = VK_TRUE;
    depthStencil.depthWriteEnable = VK_TRUE;
    depthStencil.depthCompareOp = VK_COMPARE_OP_LESS;

    VkPipelineColorBlendAttachmentState colorBlendAttachment = {};
    colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                                          VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

    VkPipelineColorBlendStateCreateInfo colorBlending = {};
    colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlending.attachmentCount = 1;
    colorBlending.pAttachments = &colorBlendAttachment;

    VkDynamicState dynamicStates[] = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
    VkPipelineDynamicStateCreateInfo dynamicState = {};
    dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamicState.dynamicStateCount = 2;
    dynamicState.pDynamicStates = dynamicStates;

    VkGraphicsPipelineCreateInfo pci = {};
    pci.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pci.stageCount = 2;
    pci.pStages = stages;
    pci.pVertexInputState = &vertexInput;
    pci.pInputAssemblyState = &inputAssembly;
    pci.pViewportState = &viewportState;
    pci.pRasterizationState = &rasterizer;
    pci.pMultisampleState = &multisampling;
    pci.pDepthStencilState = &depthStencil;
    pci.pColorBlendState = &colorBlending;
    pci.pDynamicState = &dynamicState;
    pci.layout = pipelineLayout_;
    pci.renderPass = renderPass_;
    pci.subpass = 0;

    VkPipeline pipeline;
    if (vkCreateGraphicsPipelines(device_, VK_NULL_HANDLE, 1, &pci, nullptr, &pipeline) != VK_SUCCESS) {
        std::fprintf(stderr, "Vulkan: failed to create graphics pipeline\n");
        pipeline = VK_NULL_HANDLE;
    }

    vkDestroyShaderModule(device_, vertModule, nullptr);
    vkDestroyShaderModule(device_, fragModule, nullptr);

    return pipeline;
}

bool VulkanBackend::createPipelines() {
    trianglePipeline_ = createGraphicsPipeline(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
    linePipeline_ = createGraphicsPipeline(VK_PRIMITIVE_TOPOLOGY_LINE_LIST);
    return trianglePipeline_ != VK_NULL_HANDLE && linePipeline_ != VK_NULL_HANDLE;
}

// ---------------------------------------------------------------------------
// Command pool & buffers
// ---------------------------------------------------------------------------

bool VulkanBackend::createCommandPool() {
    VkCommandPoolCreateInfo cpci = {};
    cpci.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    cpci.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    cpci.queueFamilyIndex = graphicsFamily_;

    if (vkCreateCommandPool(device_, &cpci, nullptr, &commandPool_) != VK_SUCCESS) {
        std::fprintf(stderr, "Vulkan: failed to create command pool\n");
        return false;
    }
    return true;
}

bool VulkanBackend::createCommandBuffers() {
    VkCommandBufferAllocateInfo cbai = {};
    cbai.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    cbai.commandPool = commandPool_;
    cbai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    cbai.commandBufferCount = kMaxFramesInFlight;

    if (vkAllocateCommandBuffers(device_, &cbai, commandBuffers_) != VK_SUCCESS) {
        std::fprintf(stderr, "Vulkan: failed to allocate command buffers\n");
        return false;
    }
    return true;
}

// ---------------------------------------------------------------------------
// Sync objects
// ---------------------------------------------------------------------------

bool VulkanBackend::createSyncObjects() {
    VkSemaphoreCreateInfo sci = {};
    sci.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    VkFenceCreateInfo fci = {};
    fci.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fci.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    for (int i = 0; i < kMaxFramesInFlight; ++i) {
        if (vkCreateSemaphore(device_, &sci, nullptr, &imageAvailableSems_[i]) != VK_SUCCESS ||
            vkCreateSemaphore(device_, &sci, nullptr, &renderFinishedSems_[i]) != VK_SUCCESS ||
            vkCreateFence(device_, &fci, nullptr, &inFlightFences_[i]) != VK_SUCCESS) {
            std::fprintf(stderr, "Vulkan: failed to create sync objects\n");
            return false;
        }
    }
    return true;
}

// ---------------------------------------------------------------------------
// Swapchain cleanup / recreation
// ---------------------------------------------------------------------------

void VulkanBackend::cleanupSwapchain() {
    for (auto fb : framebuffers_)
        vkDestroyFramebuffer(device_, fb, nullptr);
    framebuffers_.clear();

    if (depthImageView_) vkDestroyImageView(device_, depthImageView_, nullptr);
    depthImageView_ = VK_NULL_HANDLE;
    if (depthImage_) vkDestroyImage(device_, depthImage_, nullptr);
    depthImage_ = VK_NULL_HANDLE;
    if (depthMemory_) vkFreeMemory(device_, depthMemory_, nullptr);
    depthMemory_ = VK_NULL_HANDLE;

    for (auto iv : swapchainImageViews_)
        vkDestroyImageView(device_, iv, nullptr);
    swapchainImageViews_.clear();

    if (swapchain_) vkDestroySwapchainKHR(device_, swapchain_, nullptr);
    swapchain_ = VK_NULL_HANDLE;
}

void VulkanBackend::recreateSwapchain() {
    int w = 0, h = 0;
    glfwGetFramebufferSize(window_, &w, &h);
    while (w == 0 || h == 0) {
        glfwGetFramebufferSize(window_, &w, &h);
        glfwWaitEvents();
    }

    vkDeviceWaitIdle(device_);
    cleanupSwapchain();
    createSwapchain();
    createDepthResources();
    createFramebuffers();
}

void VulkanBackend::onResize(int /*width*/, int /*height*/) {
    framebufferResized = true;
}

// ---------------------------------------------------------------------------
// Mesh creation
// ---------------------------------------------------------------------------

VkMesh VulkanBackend::createMesh(const std::vector<float>& verts) {
    VkMesh mesh;
    mesh.vertexCount = static_cast<int>(verts.size()) / 11;

    VkDeviceSize bufferSize = verts.size() * sizeof(float);

    VkBufferCreateInfo bci = {};
    bci.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bci.size = bufferSize;
    bci.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
    bci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    if (vkCreateBuffer(device_, &bci, nullptr, &mesh.buffer) != VK_SUCCESS) {
        std::fprintf(stderr, "Vulkan: failed to create vertex buffer\n");
        return mesh;
    }

    VkMemoryRequirements memReqs;
    vkGetBufferMemoryRequirements(device_, mesh.buffer, &memReqs);

    VkMemoryAllocateInfo mai = {};
    mai.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    mai.allocationSize = memReqs.size;
    mai.memoryTypeIndex = findMemoryType(memReqs.memoryTypeBits,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    if (vkAllocateMemory(device_, &mai, nullptr, &mesh.memory) != VK_SUCCESS) {
        std::fprintf(stderr, "Vulkan: failed to allocate vertex buffer memory\n");
        return mesh;
    }

    vkBindBufferMemory(device_, mesh.buffer, mesh.memory, 0);

    void* data;
    vkMapMemory(device_, mesh.memory, 0, bufferSize, 0, &data);
    std::memcpy(data, verts.data(), static_cast<size_t>(bufferSize));
    vkUnmapMemory(device_, mesh.memory);

    return mesh;
}

void VulkanBackend::destroyMesh(VkMesh& mesh) {
    if (mesh.buffer) vkDestroyBuffer(device_, mesh.buffer, nullptr);
    if (mesh.memory) vkFreeMemory(device_, mesh.memory, nullptr);
    mesh.buffer = VK_NULL_HANDLE;
    mesh.memory = VK_NULL_HANDLE;
    mesh.vertexCount = 0;
}

// ---------------------------------------------------------------------------
// Frame rendering
// ---------------------------------------------------------------------------

bool VulkanBackend::beginFrame() {
    vkWaitForFences(device_, 1, &inFlightFences_[currentFrame_], VK_TRUE, UINT64_MAX);

    VkResult result = vkAcquireNextImageKHR(device_, swapchain_, UINT64_MAX,
        imageAvailableSems_[currentFrame_], VK_NULL_HANDLE, &imageIndex_);

    if (result == VK_ERROR_OUT_OF_DATE_KHR) {
        recreateSwapchain();
        return false;
    }
    if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
        std::fprintf(stderr, "Vulkan: failed to acquire swapchain image\n");
        return false;
    }

    vkResetFences(device_, 1, &inFlightFences_[currentFrame_]);
    vkResetCommandBuffer(commandBuffers_[currentFrame_], 0);

    VkCommandBufferBeginInfo beginInfo = {};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    vkBeginCommandBuffer(commandBuffers_[currentFrame_], &beginInfo);

    VkClearValue clearValues[2] = {};
    clearValues[0].color = {{0.1f, 0.1f, 0.12f, 1.0f}};
    clearValues[1].depthStencil = {1.0f, 0};

    VkRenderPassBeginInfo rpbi = {};
    rpbi.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    rpbi.renderPass = renderPass_;
    rpbi.framebuffer = framebuffers_[imageIndex_];
    rpbi.renderArea.extent = swapchainExtent_;
    rpbi.clearValueCount = 2;
    rpbi.pClearValues = clearValues;

    vkCmdBeginRenderPass(commandBuffers_[currentFrame_], &rpbi, VK_SUBPASS_CONTENTS_INLINE);

    // Set dynamic viewport and scissor
    VkViewport viewport = {};
    viewport.width = static_cast<float>(swapchainExtent_.width);
    viewport.height = static_cast<float>(swapchainExtent_.height);
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    vkCmdSetViewport(commandBuffers_[currentFrame_], 0, 1, &viewport);

    VkRect2D scissor = {};
    scissor.extent = swapchainExtent_;
    vkCmdSetScissor(commandBuffers_[currentFrame_], 0, 1, &scissor);

    return true;
}

void VulkanBackend::draw(const VkMesh& mesh, const float model[16], const float viewProj[16],
                          VkPrimitiveTopology topology) {
    if (mesh.vertexCount == 0 || mesh.buffer == VK_NULL_HANDLE) return;

    VkPipeline pipeline = (topology == VK_PRIMITIVE_TOPOLOGY_LINE_LIST) ? linePipeline_ : trianglePipeline_;
    vkCmdBindPipeline(commandBuffers_[currentFrame_], VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);

    PushConstants pc;
    std::memcpy(pc.model, model, 16 * sizeof(float));
    std::memcpy(pc.viewProj, viewProj, 16 * sizeof(float));
    vkCmdPushConstants(commandBuffers_[currentFrame_], pipelineLayout_,
                       VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(PushConstants), &pc);

    VkDeviceSize offset = 0;
    vkCmdBindVertexBuffers(commandBuffers_[currentFrame_], 0, 1, &mesh.buffer, &offset);
    vkCmdDraw(commandBuffers_[currentFrame_], static_cast<uint32_t>(mesh.vertexCount), 1, 0, 0);
}

void VulkanBackend::endFrame() {
    vkCmdEndRenderPass(commandBuffers_[currentFrame_]);
    vkEndCommandBuffer(commandBuffers_[currentFrame_]);

    VkPipelineStageFlags waitStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

    VkSubmitInfo submitInfo = {};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.waitSemaphoreCount = 1;
    submitInfo.pWaitSemaphores = &imageAvailableSems_[currentFrame_];
    submitInfo.pWaitDstStageMask = &waitStage;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffers_[currentFrame_];
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = &renderFinishedSems_[currentFrame_];

    if (vkQueueSubmit(graphicsQueue_, 1, &submitInfo, inFlightFences_[currentFrame_]) != VK_SUCCESS) {
        std::fprintf(stderr, "Vulkan: failed to submit draw command buffer\n");
    }

    VkPresentInfoKHR presentInfo = {};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = &renderFinishedSems_[currentFrame_];
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = &swapchain_;
    presentInfo.pImageIndices = &imageIndex_;

    VkResult result = vkQueuePresentKHR(presentQueue_, &presentInfo);
    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || framebufferResized) {
        framebufferResized = false;
        recreateSwapchain();
    }

    currentFrame_ = (currentFrame_ + 1) % kMaxFramesInFlight;
}

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

uint32_t VulkanBackend::findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties) {
    VkPhysicalDeviceMemoryProperties memProps;
    vkGetPhysicalDeviceMemoryProperties(physicalDevice_, &memProps);

    for (uint32_t i = 0; i < memProps.memoryTypeCount; ++i) {
        if ((typeFilter & (1 << i)) &&
            (memProps.memoryTypes[i].propertyFlags & properties) == properties) {
            return i;
        }
    }

    std::fprintf(stderr, "Vulkan: failed to find suitable memory type\n");
    return 0;
}
