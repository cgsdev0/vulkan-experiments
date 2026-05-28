#include <cstdint> // Necessary for uint32_t
#include <fstream>
#include <string>

#include <vulkan/vulkan_raii.hpp>
#define VK_USE_PLATFORM_WAYLAND_KHR
#include <GLFW/glfw3.h>

#define GLFW_EXPOSE_NATIVE_WAYLAND
#include <GLFW/glfw3native.h>

#include <iostream>
#include <stdexcept>

using namespace vk::raii;

int main() {
    GLFWwindow *window;

    Context context;
    Instance instance = nullptr;
    PhysicalDevice physicalDevice = nullptr;
    Device device = nullptr; // logical device
    uint32_t queueIndex = 0;
    Queue queue = nullptr; // created with our logical device
    SurfaceKHR surface = nullptr;
    SwapchainKHR swapChain = nullptr;
    std::vector<vk::Image> swapChainImages;
    vk::SurfaceFormatKHR swapChainSurfaceFormat;
    swapChainSurfaceFormat.format = (vk::Format)50;
    vk::Extent2D swapChainExtent;
    std::vector<ImageView> swapChainImageViews;
    PipelineLayout pipelineLayout = nullptr;
    Pipeline graphicsPipeline = nullptr;
    CommandPool commandPool = nullptr;
    CommandBuffer commandBuffer = nullptr;
    Semaphore presentCompleteSemaphore = nullptr;
    Semaphore renderFinishedSemaphore = nullptr;
    Fence drawFence = nullptr;
    glfwInit();
    glfwWindowHint(GLFW_CLIENT_API, 0);
    window = glfwCreateWindow(800, 600, "", nullptr, nullptr);

    auto transition_image_layout = [&](uint32_t imageIndex,
                                       vk::ImageLayout old_layout,
                                       vk::ImageLayout new_layout,
                                       vk::AccessFlags2 src_access_mask,
                                       vk::AccessFlags2 dst_access_mask,
                                       vk::PipelineStageFlags2 src_stage_mask,
                                       vk::PipelineStageFlags2 dst_stage_mask) {
        vk::ImageMemoryBarrier2 barrier = {
            .srcStageMask = src_stage_mask,
            .srcAccessMask = src_access_mask,
            .dstStageMask = dst_stage_mask,
            .dstAccessMask = dst_access_mask,
            .oldLayout = old_layout,
            .newLayout = new_layout,
            .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .image = swapChainImages[imageIndex],
            .subresourceRange = {
                .aspectMask = vk::ImageAspectFlagBits::eColor,
                .baseMipLevel = 0,
                .levelCount = 1,
                .baseArrayLayer = 0,
                .layerCount = 1}};
        vk::DependencyInfo dependencyInfo = {
            .dependencyFlags = {},
            .imageMemoryBarrierCount = 1,
            .pImageMemoryBarriers = &barrier};
        commandBuffer.pipelineBarrier2(dependencyInfo);
    };
    auto recordCommandBuffer = [&](uint32_t imageIndex) {
        commandBuffer.begin({});
        // Before starting rendering, transition the swapchain image to vk::ImageLayout::eColorAttachmentOptimal
        transition_image_layout(
            imageIndex,
            vk::ImageLayout::eUndefined,
            vk::ImageLayout::eColorAttachmentOptimal,
            {},                                                 // srcAccessMask (no need to wait for previous operations)
            vk::AccessFlagBits2::eColorAttachmentWrite,         // dstAccessMask
            vk::PipelineStageFlagBits2::eColorAttachmentOutput, // srcStage
            vk::PipelineStageFlagBits2::eColorAttachmentOutput  // dstStage
        );
        vk::ClearValue clearColor = vk::ClearColorValue(0, 0, 0, 1);
        vk::RenderingAttachmentInfo attachmentInfo = {
            .imageView = swapChainImageViews[imageIndex],
            .imageLayout = vk::ImageLayout::eColorAttachmentOptimal,
            .loadOp = vk::AttachmentLoadOp::eClear,
            .storeOp = vk::AttachmentStoreOp::eStore,
            .clearValue = clearColor};
        vk::RenderingInfo renderingInfo = {
            .renderArea = {.offset = {0, 0}, .extent = swapChainExtent},
            .layerCount = 1,
            .colorAttachmentCount = 1,
            .pColorAttachments = &attachmentInfo};
        commandBuffer.beginRendering(renderingInfo);
        commandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, *graphicsPipeline);
        commandBuffer.setViewport(0, vk::Viewport(0, 0, 800, 600, 0, 1));
        commandBuffer.setScissor(0, vk::Rect2D(vk::Offset2D(0, 0), swapChainExtent));
        commandBuffer.draw(3, 1, 0, 0);
        commandBuffer.endRendering();
        // After rendering, transition the swapchain image to vk::ImageLayout::ePresentSrcKHR
        transition_image_layout(
            imageIndex,
            vk::ImageLayout::eColorAttachmentOptimal,
            vk::ImageLayout::ePresentSrcKHR,
            vk::AccessFlagBits2::eColorAttachmentWrite,         // srcAccessMask
            {},                                                 // dstAccessMask
            vk::PipelineStageFlagBits2::eColorAttachmentOutput, // srcStage
            vk::PipelineStageFlagBits2::eBottomOfPipe           // dstStage
        );
        commandBuffer.end();
    };
    // createInstance
    {
        constexpr vk::ApplicationInfo appInfo{
            .pApplicationName = "Hello Triangle",
            .apiVersion = vk::ApiVersion14};

        uint32_t glfwExtensionCount = 0;
        auto glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

        vk::InstanceCreateInfo createInfo{
            .pApplicationInfo = &appInfo,
            .enabledExtensionCount = glfwExtensionCount,
            .ppEnabledExtensionNames = glfwExtensions};
        instance = Instance(context, createInfo);
    }

    // createSurface
    {
        VkSurfaceKHR _surface;
        glfwCreateWindowSurface(*instance, window, nullptr, &_surface);
        surface = SurfaceKHR(instance, _surface);
    }

    // pickPhysicalDevice
    physicalDevice = instance.enumeratePhysicalDevices()[0];
    // createLogicalDevice()
    {
        std::vector<const char *> requiredDeviceExtension = {
            vk::KHRSwapchainExtensionName};
        // query for Vulkan 1.3 features
        vk::StructureChain<vk::PhysicalDeviceFeatures2, vk::PhysicalDeviceVulkan13Features, vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT> featureChain = {
            {},                            // vk::PhysicalDeviceFeatures2
            {.dynamicRendering = true},    // vk::PhysicalDeviceVulkan13Features
            {.extendedDynamicState = true} // vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT
        };

        // create a Device
        float queuePriority = 0.5f;
        vk::DeviceQueueCreateInfo deviceQueueCreateInfo{.queueFamilyIndex = queueIndex, .queueCount = 1, .pQueuePriorities = &queuePriority};
        vk::DeviceCreateInfo deviceCreateInfo{
            .pNext = &featureChain.get<vk::PhysicalDeviceFeatures2>(),
            .queueCreateInfoCount = 1,
            .pQueueCreateInfos = &deviceQueueCreateInfo,
            .enabledExtensionCount = static_cast<uint32_t>(requiredDeviceExtension.size()),
            .ppEnabledExtensionNames = requiredDeviceExtension.data()};

        device = Device(physicalDevice, deviceCreateInfo);
        queue = Queue(device, queueIndex, 0);
    }
    // createSwapChain
    {
        vk::SurfaceCapabilitiesKHR surfaceCapabilities = physicalDevice.getSurfaceCapabilitiesKHR(*surface);
        swapChainExtent.setWidth(800).setHeight(600);
        uint32_t minImageCount = 3;

        vk::SwapchainCreateInfoKHR swapChainCreateInfo{
            .surface = *surface,
            .minImageCount = minImageCount,
            .imageFormat = swapChainSurfaceFormat.format,
            .imageColorSpace = swapChainSurfaceFormat.colorSpace,
            .imageExtent = swapChainExtent,
            .imageArrayLayers = 1,
            .imageUsage = vk::ImageUsageFlagBits::eColorAttachment,
            .imageSharingMode = vk::SharingMode::eExclusive,
            .preTransform = surfaceCapabilities.currentTransform,
            .compositeAlpha = vk::CompositeAlphaFlagBitsKHR::eOpaque,
            .presentMode = (vk::PresentModeKHR)1,
            .clipped = true};
        swapChainCreateInfo.oldSwapchain = nullptr;
        swapChain = SwapchainKHR(device, swapChainCreateInfo);
        swapChainImages = swapChain.getImages();
    }

    // createImageViews
    {
        vk::ImageViewCreateInfo imageViewCreateInfo{
            .viewType = vk::ImageViewType::e2D,
            .format = swapChainSurfaceFormat.format,
            .subresourceRange = {vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1}};

        // default swizzling (unnecessary)
        imageViewCreateInfo.components = {
            vk::ComponentSwizzle::eIdentity, vk::ComponentSwizzle::eIdentity, vk::ComponentSwizzle::eIdentity, vk::ComponentSwizzle::eIdentity};
        imageViewCreateInfo.subresourceRange = {.aspectMask = vk::ImageAspectFlagBits::eColor, .levelCount = 1, .layerCount = 1};
        for (auto &image : swapChainImages) {
            imageViewCreateInfo.image = image;
            swapChainImageViews.emplace_back(device, imageViewCreateInfo);
        }
    }

    // createGraphicsPipeline
    {
        std::string code = R"(
static float2 positions[3] = float2[](
    float2(0.0, -0.5),
    float2(0.5, 0.5),
    float2(-0.5, 0.5)
);

static float3 colors[3] = float3[](
    float3(1.0, 0.0, 0.0),
    float3(0.0, 1.0, 0.0),
    float3(0.0, 0.0, 1.0)
);

struct VertexOutput {
    float3 color;
    float4 sv_position : SV_Position;
};

[shader("vertex ")]
    VertexOutput
    vertMain(uint vid : SV_VertexID) {
    VertexOutput output;
    output.sv_position = float4(positions[vid], 0.0, 1.0);
    output.color = colors[vid];
    return output;
}

[shader("fragment")] float4 fragMain(VertexOutput inVert) : SV_Target {
    float3 color = inVert.color;
    return float4(color, 1.0);
}
)";
        vk::ShaderModuleCreateInfo createInfo{
            .codeSize = code.size(),
            .pCode = reinterpret_cast<const uint32_t *>(code.data())};
        ShaderModule shaderModule{device, createInfo};

        vk::PipelineShaderStageCreateInfo vertShaderStageInfo{
            .stage = vk::ShaderStageFlagBits::eVertex,
            .module = shaderModule,
            .pName = "vertMain"};

        vk::PipelineShaderStageCreateInfo fragShaderStageInfo{
            .stage = vk::ShaderStageFlagBits::eFragment,
            .module = shaderModule,
            .pName = "fragMain"};

        vk::PipelineShaderStageCreateInfo shaderStages[] = {
            vertShaderStageInfo,
            fragShaderStageInfo};

        std::vector<vk::DynamicState> dynamicStates = {
            vk::DynamicState::eViewport,
            vk::DynamicState::eScissor};

        vk::PipelineDynamicStateCreateInfo dynamicState{
            .dynamicStateCount = static_cast<uint32_t>(dynamicStates.size()),
            .pDynamicStates = dynamicStates.data()};

        vk::PipelineVertexInputStateCreateInfo vertexInputInfo;

        vk::PipelineInputAssemblyStateCreateInfo inputAssembly{
            .topology = vk::PrimitiveTopology::eTriangleList};

        vk::Viewport viewport{
            0,
            0,
            (float)swapChainExtent.width,
            (float)swapChainExtent.height,
            0,
            1};

        vk::Rect2D scissor{vk::Offset2D{0, 0}, swapChainExtent};
        vk::PipelineViewportStateCreateInfo viewportState{
            .viewportCount = 1,
            .pViewports = &viewport,
            .scissorCount = 1,
            .pScissors = &scissor};

        vk::PipelineColorBlendAttachmentState colorBlendAttachment{
            .blendEnable = vk::False,
            .colorWriteMask = vk::ColorComponentFlagBits(15)};

        vk::PipelineColorBlendStateCreateInfo colorBlending{
            .logicOpEnable = vk::False,
            .logicOp = vk::LogicOp::eCopy};
        colorBlending.setAttachments({colorBlendAttachment});

        vk::PipelineRasterizationStateCreateInfo rasterizer{
            .depthClampEnable = vk::False,
            .rasterizerDiscardEnable = vk::False,
            .polygonMode = vk::PolygonMode::eFill,
            .cullMode = vk::CullModeFlagBits::eBack,
            .frontFace = vk::FrontFace::eClockwise,
            .depthBiasEnable = vk::False,
            .lineWidth = 1};

        vk::PipelineMultisampleStateCreateInfo multisampling{
            .rasterizationSamples = vk::SampleCountFlagBits::e1,
            .sampleShadingEnable = vk::False};

        vk::PipelineLayoutCreateInfo pipelineLayoutInfo{
            .setLayoutCount = 0,
            .pushConstantRangeCount = 0};

        pipelineLayout = PipelineLayout(device, pipelineLayoutInfo);

        vk::PipelineRenderingCreateInfo pipelineRenderingCreateInfo{
            .colorAttachmentCount = 1,
            .pColorAttachmentFormats = &swapChainSurfaceFormat.format};

        vk::StructureChain<vk::GraphicsPipelineCreateInfo, vk::PipelineRenderingCreateInfo> pipelineCreateInfoChain = {
            {.stageCount = 2,
             .pStages = shaderStages,
             .pVertexInputState = &vertexInputInfo,
             .pInputAssemblyState = &inputAssembly,
             .pViewportState = &viewportState,
             .pRasterizationState = &rasterizer,
             .pMultisampleState = &multisampling,
             .pColorBlendState = &colorBlending,
             .pDynamicState = &dynamicState,
             .layout = pipelineLayout,
             .renderPass = nullptr},
            {.colorAttachmentCount = 1,
             .pColorAttachmentFormats = &swapChainSurfaceFormat.format}};

        graphicsPipeline = Pipeline(device, nullptr, pipelineCreateInfoChain.get<vk::GraphicsPipelineCreateInfo>());
    }
    // createCommandPool
    {
        vk::CommandPoolCreateInfo poolInfo{
            .flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer,
            .queueFamilyIndex = queueIndex};

        commandPool = CommandPool(device, poolInfo);
    }

    // createCommandBuffer()
    {
        vk::CommandBufferAllocateInfo allocInfo{
            .commandPool = commandPool,
            .level = vk::CommandBufferLevel::ePrimary,
            .commandBufferCount = 1};

        commandBuffer = std::move(CommandBuffers(device, allocInfo).front());
    }

    // createSyncObjects()
    {
        presentCompleteSemaphore = Semaphore(device, vk::SemaphoreCreateInfo());
        renderFinishedSemaphore = Semaphore(device, vk::SemaphoreCreateInfo());
        drawFence = Fence(device, {.flags = vk::FenceCreateFlagBits::eSignaled});
    }

    // drawFrame()
    {
        auto fenceResult = device.waitForFences(*drawFence, vk::True, UINT64_MAX);
        device.resetFences(*drawFence);
        auto [result, imageIndex] = swapChain.acquireNextImage(UINT64_MAX, *presentCompleteSemaphore, nullptr);
        recordCommandBuffer(imageIndex);
        vk::PipelineStageFlags waitDestinationStageMask(vk::PipelineStageFlagBits::eColorAttachmentOutput);
        const vk::SubmitInfo submitInfo{
            .waitSemaphoreCount = 1,
            .pWaitSemaphores = &*presentCompleteSemaphore,
            .pWaitDstStageMask = &waitDestinationStageMask,
            .commandBufferCount = 1,
            .pCommandBuffers = &*commandBuffer,
            .signalSemaphoreCount = 1,
            .pSignalSemaphores = &*renderFinishedSemaphore};
        queue.submit(submitInfo, *drawFence);
        const vk::PresentInfoKHR presentInfoKHR{
            .waitSemaphoreCount = 1,
            .pWaitSemaphores = &*renderFinishedSemaphore,
            .swapchainCount = 1,
            .pSwapchains = &*swapChain,
            .pImageIndices = &imageIndex};
        result = queue.presentKHR(presentInfoKHR);
    }
    while (1) {
    }
}
