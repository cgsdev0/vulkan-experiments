#if defined(__INTELLISENSE__) || !defined(USE_CPP20_MODULES)
#include <vulkan/vulkan_raii.hpp>
#else
import vulkan_hpp;
#endif
#include <GLFW/glfw3.h>

#include <cstdlib>
#include <iostream>
#include <stdexcept>

const std::vector<char const *> validationLayers = {
    "VK_LAYER_KHRONOS_validation"};

#ifdef NDEBUG
constexpr bool enableValidationLayers = false;
#else
constexpr bool enableValidationLayers = true;
#endif

class HelloTriangleApplication {
  public:
    void run() {
        initVulkan();
        mainLoop();
        cleanup();
    }

  private:
    vk::raii::Context context;
    vk::raii::Instance instance = nullptr;
    vk::raii::PhysicalDevice physicalDevice = nullptr;
    vk::raii::Device device = nullptr;       // logical device
    vk::raii::Queue graphicsQueue = nullptr; // created with our logical device

    void initVulkan() {
        createInstance();
        pickPhysicalDevice();
        createLogicalDevice();
    }

    void createLogicalDevice() {
        std::vector<vk::QueueFamilyProperties> queueFamilyProperties = physicalDevice.getQueueFamilyProperties();
        auto graphicsQueueFamilyProperty = std::ranges::find_if(queueFamilyProperties, [](auto const &qfp) { return (qfp.queueFlags & vk::QueueFlagBits::eGraphics) != static_cast<vk::QueueFlags>(0); });
        auto graphicsIndex = static_cast<uint32_t>(std::distance(queueFamilyProperties.begin(), graphicsQueueFamilyProperty));
        float queuePriority = 0.5f;
        vk::DeviceQueueCreateInfo deviceQueueCreateInfo{.queueFamilyIndex = graphicsIndex, .queueCount = 1, .pQueuePriorities = &queuePriority};
        vk::PhysicalDeviceFeatures deviceFeatures;
        /* Vulkan is designed to be backwards compatible, which means that by default,
         * you only get access to the basic features that were available in Vulkan 1.0.
         * To use newer features, you need to explicitly request them during device creation.
         *
         * Create a chain of feature structures
         */
        vk::StructureChain<vk::PhysicalDeviceFeatures2, vk::PhysicalDeviceVulkan13Features, vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT> featureChain = {
            {},                            // vk::PhysicalDeviceFeatures2 (empty for now)
            {.dynamicRendering = true},    // Enable dynamic rendering from Vulkan 1.3
            {.extendedDynamicState = true} // Enable extended dynamic state from the extension
        };
        std::vector<const char *> requiredDeviceExtension = {
            vk::KHRSwapchainExtensionName};
        vk::DeviceCreateInfo deviceCreateInfo{
            .pNext = &featureChain.get<vk::PhysicalDeviceFeatures2>(),
            .queueCreateInfoCount = 1,
            .pQueueCreateInfos = &deviceQueueCreateInfo,
            .enabledExtensionCount = static_cast<uint32_t>(requiredDeviceExtension.size()),
            .ppEnabledExtensionNames = requiredDeviceExtension.data()};

        device = vk::raii::Device(physicalDevice, deviceCreateInfo);
        graphicsQueue = vk::raii::Queue(device, graphicsIndex, 0);
    }

    bool isDeviceSuitable(vk::raii::PhysicalDevice const &physicalDevice) {
        auto deviceProperties = physicalDevice.getProperties();
        auto deviceFeatures = physicalDevice.getFeatures();

        bool supportsVulkan1_3 = physicalDevice.getProperties().apiVersion >= vk::ApiVersion13;
        auto queueFamilies = physicalDevice.getQueueFamilyProperties();
        bool supportsGraphics =
            std::ranges::any_of(queueFamilies, [](auto const &qfp) { return !!(qfp.queueFlags & vk::QueueFlagBits::eGraphics); });
        std::vector<const char *> requiredDeviceExtension = {vk::KHRSwapchainExtensionName};

        auto availableDeviceExtensions = physicalDevice.enumerateDeviceExtensionProperties();
        bool supportsAllRequiredExtensions =
            std::ranges::all_of(requiredDeviceExtension,
                                [&availableDeviceExtensions](auto const &requiredDeviceExtension) {
                                    return std::ranges::any_of(availableDeviceExtensions,
                                                               [requiredDeviceExtension](auto const &availableDeviceExtension) { return strcmp(availableDeviceExtension.extensionName, requiredDeviceExtension) == 0; });
                                });
        auto features = physicalDevice.template getFeatures2<vk::PhysicalDeviceFeatures2, vk::PhysicalDeviceVulkan13Features, vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT>();
        bool supportsRequiredFeatures = features.template get<vk::PhysicalDeviceVulkan13Features>().dynamicRendering &&
                                        features.template get<vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT>().extendedDynamicState;

        // std::cout << "supportsRequiredFeatures: " << supportsRequiredFeatures << std::endl;
        // std::cout << "supportsAllRequiredExtensions: " << supportsAllRequiredExtensions << std::endl;
        // std::cout << "supportsVulkan1_3: " << supportsVulkan1_3 << std::endl;
        // std::cout << "supportsGraphics: " << supportsGraphics << std::endl;
        // std::cout << "deviceType: " << (int)deviceProperties.deviceType << std::endl;
        // std::cout << "geometryShader: " << deviceFeatures.geometryShader << std::endl;
        if (!supportsRequiredFeatures) return false;
        if (!supportsAllRequiredExtensions) return false;
        if (!supportsVulkan1_3) return false;
        if (!supportsGraphics) return false;
        if (!deviceFeatures.geometryShader) return false;
        return true;
    }

    // Choose our actual graphics card
    void pickPhysicalDevice() {
        auto physicalDevices = instance.enumeratePhysicalDevices();
        if (physicalDevices.empty()) {
            throw std::runtime_error("failed to find GPUs with Vulkan support!");
        }
        std::cout << "selected device: ";
        for (auto device : physicalDevices) {

            auto props = device.getProperties();
            bool suitable = isDeviceSuitable(device);
            if (suitable) {
                std::cout << props.deviceName << std::endl;
                if (this->physicalDevice == nullptr) {
                    this->physicalDevice = device;
                    break;
                }
            }
        }
        if (this->physicalDevice == nullptr) {
            throw std::runtime_error("no suitable GPU found!");
        }
    }

    void createInstance() {
        // Get the required layers
        std::vector<char const *> requiredLayers;
        if (enableValidationLayers) {
            requiredLayers.assign(validationLayers.begin(), validationLayers.end());
        }

        // Check if the required layers are supported by the Vulkan implementation.
        auto layerProperties = context.enumerateInstanceLayerProperties();
        auto unsupportedLayerIt = std::ranges::find_if(requiredLayers,
                                                       [&layerProperties](auto const &requiredLayer) {
                                                           return std::ranges::none_of(layerProperties,
                                                                                       [requiredLayer](auto const &layerProperty) { return strcmp(layerProperty.layerName, requiredLayer) == 0; });
                                                       });
        if (unsupportedLayerIt != requiredLayers.end()) {
            throw std::runtime_error("Required layer not supported: " + std::string(*unsupportedLayerIt));
        }
        constexpr vk::ApplicationInfo appInfo{
            .pApplicationName = "Hello Triangle",
            .applicationVersion = VK_MAKE_VERSION(1, 0, 0),
            .pEngineName = "No Engine",
            .engineVersion = VK_MAKE_VERSION(1, 0, 0),
            .apiVersion = vk::ApiVersion14};

        auto extensions = context.enumerateInstanceExtensionProperties();
        std::cout << "available extensions:\n";

        for (const auto &extension : extensions) {
            std::cout << '\t' << extension.extensionName << '\n';
        }
        // Get the required instance extensions from GLFW.
        uint32_t glfwExtensionCount = 0;
        auto glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

        // Check if the required GLFW extensions are supported by the Vulkan implementation.
        auto extensionProperties = context.enumerateInstanceExtensionProperties();
        for (uint32_t i = 0; i < glfwExtensionCount; ++i) {
            if (std::ranges::none_of(extensionProperties,
                                     [glfwExtension = glfwExtensions[i]](auto const &extensionProperty) { return strcmp(extensionProperty.extensionName, glfwExtension) == 0; })) {
                throw std::runtime_error("Required GLFW extension not supported: " + std::string(glfwExtensions[i]));
            }
        }

        vk::InstanceCreateInfo createInfo{
            .pApplicationInfo = &appInfo,
            .enabledExtensionCount = glfwExtensionCount,
            .ppEnabledExtensionNames = glfwExtensions};
        instance = vk::raii::Instance(context, createInfo);
    }

    void mainLoop() {
    }

    void cleanup() {
    }
};

int main() {
    try {
        HelloTriangleApplication app;
        app.run();
    } catch (const std::exception &e) {
        std::cerr << e.what() << std::endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
