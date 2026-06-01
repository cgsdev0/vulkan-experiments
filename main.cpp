#include <bits/stdc++.h>

#include <vulkan/vulkan_raii.hpp>
#define VK_USE_PLATFORM_WAYLAND_KHR
#include <GLFW/glfw3.h>

#define uint unsigned int

using namespace std;
using namespace vk;

int main() {

    raii::Context context;

    glfwInit();
    glfwWindowHint(GLFW_CLIENT_API, 0);
    GLFWwindow *window =
        glfwCreateWindow(800, 600, "", nullptr, nullptr);

    ApplicationInfo app;
    app.apiVersion = ApiVersion14;

    uint count = 0;
    auto ext = glfwGetRequiredInstanceExtensions(&count);

    auto instance = raii::Instance(
        context, {.pApplicationInfo = &app,
                  .enabledExtensionCount = count,
                  .ppEnabledExtensionNames = ext});

    VkSurfaceKHR surface;
    glfwCreateWindowSurface(*instance, window, nullptr,
                            &surface);

    // skip 1
    auto physDev = instance.enumeratePhysicalDevices()[0];

    vector<const char *> required = {
        KHRSwapchainExtensionName};
    StructureChain<PhysicalDeviceFeatures2,
                   PhysicalDeviceVulkan13Features>
        featureChain = {
            {},
            {.dynamicRendering = true},
        };

    // create a Device
    float qp = 0;
    DeviceQueueCreateInfo qInfo;
    qInfo.setQueuePriorities(qp);
    DeviceCreateInfo devInfo{
        .pNext =
            &featureChain.get<PhysicalDeviceFeatures2>(),
        .enabledExtensionCount = (uint)required.size(),
        .ppEnabledExtensionNames = required.data()};
    devInfo.setQueueCreateInfos(qInfo);

    auto dev = raii::Device(physDev, devInfo);
    auto queue = raii::Queue(dev, 0, 0);
    auto surfaceCapabilities =
        physDev.getSurfaceCapabilitiesKHR(surface);

    SurfaceFormatKHR fmt{.format = (Format)50};
    Extent2D e(800, 600);

    SwapchainCreateInfoKHR info{
        .surface = surface,
        .minImageCount = 3,
        .imageFormat = fmt.format,
        .imageColorSpace = fmt.colorSpace,
        .imageExtent = e,
        .imageArrayLayers = 1,
        .imageUsage = ImageUsageFlagBits::eColorAttachment,
        .imageSharingMode = SharingMode::eExclusive,
        .preTransform =
            surfaceCapabilities.currentTransform,
        .compositeAlpha =
            CompositeAlphaFlagBitsKHR::eOpaque,
        .presentMode = (PresentModeKHR)1,
        .clipped = true};
    auto swapchain = raii::SwapchainKHR(dev, info);
    auto images = swapchain.getImages();
    vector<raii::ImageView> views;
    for (auto &image : images) {
        ImageViewCreateInfo iv{
            .image = image,
            .viewType = ImageViewType::e2D,
            .format = fmt.format,
            .subresourceRange = {
                ImageAspectFlagBits::eColor, 0, 1, 0, 1}};
        views.emplace_back(dev, iv);
    }

    auto f = ifstream("shaders/slang.spv", ios::binary);
    vector<char> code(istreambuf_iterator<char>(f), {});
    ShaderModuleCreateInfo createInfo{
        .codeSize = code.size(),
        .pCode = (uint *)(code.data())};

    raii::ShaderModule shaderModule{dev, createInfo};

    PipelineShaderStageCreateInfo shaderStages[] = {
        {.stage = ShaderStageFlagBits::eVertex,
         .module = shaderModule,
         .pName = "vertMain"},
        {.stage = ShaderStageFlagBits::eFragment,
         .module = shaderModule,
         .pName = "fragMain"}};

    vector<DynamicState> dynamicStates = {
        DynamicState::eViewport, DynamicState::eScissor};

    PipelineDynamicStateCreateInfo dynamicState;
    dynamicState.setDynamicStates(dynamicStates);

    PipelineInputAssemblyStateCreateInfo inputAssembly{
        .topology = PrimitiveTopology::eTriangleList};

    Viewport viewport(0, 0, 800, 600);
    Rect2D scissor{Offset2D{0, 0}, e};

    PipelineViewportStateCreateInfo vp;
    vp.setViewports(viewport);
    vp.setScissors(scissor);

    PipelineColorBlendAttachmentState colorBlendAttachment{
        .colorWriteMask = ColorComponentFlagBits(15)};

    PipelineColorBlendStateCreateInfo blend;
    blend.setAttachments({colorBlendAttachment});

    PipelineRasterizationStateCreateInfo rs;
    PipelineMultisampleStateCreateInfo ms;

    auto pipelineLayout = raii::PipelineLayout(dev, {});

    PipelineRenderingCreateInfo pri;
    pri.setColorAttachmentFormats(fmt.format);

    auto graphicsPipeline = raii::Pipeline(
        dev, nullptr,
        {.pNext = &pri,
         .stageCount = 2,
         .pStages = shaderStages,
         .pInputAssemblyState = &inputAssembly,
         .pViewportState = &vp,
         .pRasterizationState = &rs,
         .pMultisampleState = &ms,
         .pColorBlendState = &blend,
         .pDynamicState = &dynamicState,
         .layout = pipelineLayout});

    auto commandPool = raii::CommandPool(
        dev, {.flags = CommandPoolCreateFlagBits::
                  eResetCommandBuffer});

    auto cbuf =
        std::move(raii::CommandBuffers(
                      dev, {.commandPool = commandPool,
                            .commandBufferCount = 1})
                      .front());
    auto [result, idx] = swapchain.acquireNextImage(
        UINT64_MAX, nullptr, nullptr);
    cbuf.begin({});
    RenderingAttachmentInfo attachmentInfo{.imageView =
                                               views[idx]};
    RenderingInfo renderingInfo = {
        .renderArea = {.extent = e}};
    renderingInfo.setColorAttachments(attachmentInfo);
    cbuf.beginRendering(renderingInfo);
    cbuf.bindPipeline(PipelineBindPoint::eGraphics,
                      *graphicsPipeline);
    cbuf.setViewport(0, viewport);
    cbuf.setScissor(0, scissor);
    cbuf.draw(3, 1, 0, 0);
    cbuf.endRendering();
    cbuf.end();
    SubmitInfo s;
    queue.submit(s.setCommandBuffers(*cbuf), nullptr);
    result = queue.presentKHR({.swapchainCount = 1,
                               .pSwapchains = &*swapchain,
                               .pImageIndices = &idx});
    while (1)
        ;
}
