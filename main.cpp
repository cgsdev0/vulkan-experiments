#include <bits/stdc++.h>

#include <vulkan/vulkan_raii.hpp>
#define VK_USE_PLATFORM_WAYLAND_KHR
#include <GLFW/glfw3.h>
#define GLFW_EXPOSE_NATIVE_WAYLAND
#include <GLFW/glfw3native.h>

#define uint unsigned int

using namespace std;
using namespace vk;

int main() {
    GLFWwindow *window;

    raii::Context context;
    raii::PhysicalDevice physDev = nullptr;
    raii::Device dev = nullptr;  // logical device
    raii::Queue queue = nullptr; // created with our logical device
    raii::SwapchainKHR swapChain = nullptr;
    vector<Image> swapChainImages;
    SurfaceFormatKHR fmt{.format = (Format)50};
    Extent2D e;
    vector<raii::ImageView> swapChainImageViews;
    raii::Pipeline graphicsPipeline = nullptr;
    raii::CommandPool commandPool = nullptr;
    glfwInit();
    glfwWindowHint(GLFW_CLIENT_API, 0);
    window = glfwCreateWindow(800, 600, "", nullptr, nullptr);

    constexpr ApplicationInfo appInfo{
        .apiVersion = ApiVersion14};

    uint count = 0;
    auto ext = glfwGetRequiredInstanceExtensions(&count);

    auto instance = raii::Instance(
        context,
        {.pApplicationInfo = &appInfo,
         .enabledExtensionCount = count,
         .ppEnabledExtensionNames = ext});

    VkSurfaceKHR surface;
    glfwCreateWindowSurface(*instance, window, nullptr, &surface);

    // pickPhysicalDevice
    physDev = instance.enumeratePhysicalDevices()[0];

    // createLogicalDevice()
    {
        vector<const char *> requiredDeviceExtension = {
            KHRSwapchainExtensionName};
        StructureChain<PhysicalDeviceFeatures2, PhysicalDeviceVulkan13Features> featureChain = {
            {},
            {.dynamicRendering = true},
        };

        // create a Device
        float queuePriority = 0.5f;
        DeviceQueueCreateInfo deviceQueueCreateInfo{.queueCount = 1, .pQueuePriorities = &queuePriority};
        DeviceCreateInfo deviceCreateInfo{
            .pNext = &featureChain.get<PhysicalDeviceFeatures2>(),
            .queueCreateInfoCount = 1,
            .pQueueCreateInfos = &deviceQueueCreateInfo,
            .enabledExtensionCount = (uint)requiredDeviceExtension.size(),
            .ppEnabledExtensionNames = requiredDeviceExtension.data()};

        dev = raii::Device(physDev, deviceCreateInfo);
        queue = raii::Queue(dev, 0, 0);
    }
    // createSwapChain
    {
        SurfaceCapabilitiesKHR surfaceCapabilities = physDev.getSurfaceCapabilitiesKHR(surface);
        e.setWidth(800).setHeight(600);
        uint minImageCount = 3;

        SwapchainCreateInfoKHR swapChainCreateInfo{
            .surface = surface,
            .minImageCount = minImageCount,
            .imageFormat = fmt.format,
            .imageColorSpace = fmt.colorSpace,
            .imageExtent = e,
            .imageArrayLayers = 1,
            .imageUsage = ImageUsageFlagBits::eColorAttachment,
            .imageSharingMode = SharingMode::eExclusive,
            .preTransform = surfaceCapabilities.currentTransform,
            .compositeAlpha = CompositeAlphaFlagBitsKHR::eOpaque,
            .presentMode = (PresentModeKHR)1,
            .clipped = true};
        swapChainCreateInfo.oldSwapchain = nullptr;
        swapChain = raii::SwapchainKHR(dev, swapChainCreateInfo);
        swapChainImages = swapChain.getImages();
    }

    // createImageViews
    {
        ImageViewCreateInfo imageViewCreateInfo{
            .viewType = ImageViewType::e2D,
            .format = fmt.format,
            .subresourceRange = {ImageAspectFlagBits::eColor, 0, 1, 0, 1}};

        imageViewCreateInfo.subresourceRange = {.aspectMask = ImageAspectFlagBits::eColor, .levelCount = 1, .layerCount = 1};
        for (auto &image : swapChainImages) {
            imageViewCreateInfo.image = image;
            swapChainImageViews.emplace_back(dev, imageViewCreateInfo);
        }
    }

    // createGraphicsPipeline
    auto f = ifstream("shaders/slang.spv", ios::binary);
    vector<char> code(istreambuf_iterator<char>(f), {});
    ShaderModuleCreateInfo createInfo{
        .codeSize = code.size(),
        .pCode = reinterpret_cast<const uint *>(code.data())};

    raii::ShaderModule shaderModule{dev, createInfo};

    PipelineShaderStageCreateInfo shaderStages[] = {
        {.stage = ShaderStageFlagBits::eVertex,
         .module = shaderModule,
         .pName = "vertMain"},
        {.stage = ShaderStageFlagBits::eFragment,
         .module = shaderModule,
         .pName = "fragMain"}};

    vector<DynamicState> dynamicStates = {
        DynamicState::eViewport,
        DynamicState::eScissor};

    PipelineDynamicStateCreateInfo dynamicState{
        .dynamicStateCount = static_cast<uint>(dynamicStates.size()),
        .pDynamicStates = dynamicStates.data()};

    PipelineVertexInputStateCreateInfo vertexInputInfo;

    PipelineInputAssemblyStateCreateInfo inputAssembly{
        .topology = PrimitiveTopology::eTriangleList};

    Viewport viewport{
        .width = (float)e.width,
        .height = (float)e.height};

    Rect2D scissor{Offset2D{0, 0}, e};
    PipelineViewportStateCreateInfo vp;
    vp.setViewports(viewport);
    vp.setScissors(scissor);

    PipelineColorBlendAttachmentState colorBlendAttachment{
        .colorWriteMask = ColorComponentFlagBits(15)};

    PipelineColorBlendStateCreateInfo blend;
    blend.logicOp = LogicOp::eCopy;
    blend.setAttachments({colorBlendAttachment});

    PipelineRasterizationStateCreateInfo rasterizer{
        .polygonMode = PolygonMode::eFill,
        .cullMode = CullModeFlagBits::eBack,
        .frontFace = FrontFace::eClockwise,
        .lineWidth = 1};

    PipelineMultisampleStateCreateInfo multisampling{
        .rasterizationSamples = SampleCountFlagBits::e1,
    };

    auto pipelineLayout = raii::PipelineLayout(dev, {});

    StructureChain<GraphicsPipelineCreateInfo, PipelineRenderingCreateInfo> pipelineCreateInfoChain = {
        {.stageCount = 2,
         .pStages = shaderStages,
         .pVertexInputState = &vertexInputInfo,
         .pInputAssemblyState = &inputAssembly,
         .pViewportState = &vp,
         .pRasterizationState = &rasterizer,
         .pMultisampleState = &multisampling,
         .pColorBlendState = &blend,
         .pDynamicState = &dynamicState,
         .layout = pipelineLayout,
         .renderPass = nullptr},
        {.colorAttachmentCount = 1,
         .pColorAttachmentFormats = &fmt.format}};

    graphicsPipeline = raii::Pipeline(dev, nullptr, pipelineCreateInfoChain.get<GraphicsPipelineCreateInfo>());

    commandPool = raii::CommandPool(
        dev,
        {.flags = CommandPoolCreateFlagBits::eResetCommandBuffer});

    auto cbuf = std::move(
        raii::CommandBuffers(
            dev,
            {.commandPool = commandPool,
             .level = CommandBufferLevel::ePrimary,
             .commandBufferCount = 1})
            .front());
    auto [result, imageIndex] = swapChain.acquireNextImage(UINT64_MAX, nullptr, nullptr);
    cbuf.begin({});
    ClearValue clearColor = ClearColorValue(0, 0, 0, 1);
    RenderingAttachmentInfo attachmentInfo{
        .imageView = swapChainImageViews[imageIndex],
        .imageLayout = ImageLayout::eColorAttachmentOptimal,
        .loadOp = AttachmentLoadOp::eClear,
        .clearValue = clearColor};
    RenderingInfo renderingInfo = {
        .renderArea = {.extent = e},
        .layerCount = 1};
    renderingInfo.setColorAttachments(attachmentInfo);
    cbuf.beginRendering(renderingInfo);
    cbuf.bindPipeline(PipelineBindPoint::eGraphics, *graphicsPipeline);
    cbuf.setViewport(0, viewport);
    cbuf.setScissor(0, scissor);
    cbuf.draw(3, 1, 0, 0);
    cbuf.endRendering();
    cbuf.end();
    PipelineStageFlags waitDestinationStageMask(PipelineStageFlagBits::eColorAttachmentOutput);
    const SubmitInfo submitInfo{
        .commandBufferCount = 1,
        .pCommandBuffers = &*cbuf};
    queue.submit(submitInfo, nullptr);
    const PresentInfoKHR presentInfoKHR{
        .swapchainCount = 1,
        .pSwapchains = &*swapChain,
        .pImageIndices = &imageIndex};
    result = queue.presentKHR(presentInfoKHR);
    while (1)
        ;
}
