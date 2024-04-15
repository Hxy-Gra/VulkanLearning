#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <iostream>
#include <stdexcept>
#include <cstdlib>
#include <limits>
#include <algorithm>
#include <vector>
#include <string>
#include <optional>
#include <set>
#include <fstream>

const uint32_t WIDTH = 800;
const uint32_t HEIGHT = 600;

const int MAX_FRAMES_IN_FLIGHT = 2;

//指定全局变量，启用验证层的名称
const std::vector<const char*> validationLayers = {
    "VK_LAYER_KHRONOS_validation"
};

//指定全局变量，Device扩展列表
const std::vector<const char*> deviceExtensions = {
    VK_KHR_SWAPCHAIN_EXTENSION_NAME
};

#ifdef NDEBUG   //如果没有启用debug模式，则validation layer不启用
    const bool enableValidationLayers = false;
#else
    const bool enableValidationLayers = true;
#endif

/* 
自定义代理函数，用于查找不会被自动加载的扩展函数CreateDebugUtilsMessengerEXT 和 DestroyDebugUtilsMessengerEXT
*/
VkResult CreateDebugUtilsMessengerEXT(VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDebugUtilsMessengerEXT* pDebugMessenger) {
    auto func = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");
    if (func != nullptr) {
        return func(instance, pCreateInfo, pAllocator, pDebugMessenger);
    }
    else {
        return VK_ERROR_EXTENSION_NOT_PRESENT;
    }
}

void DestroyDebugUtilsMessengerEXT(VkInstance instance, VkDebugUtilsMessengerEXT debugMessenger, const VkAllocationCallbacks* pAllocator) {
    auto func = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");
    if (func != nullptr) {
        func(instance, debugMessenger, pAllocator);
    }
}

//辅助函数，从文件中加载二进制数据
static std::vector<char> readFile(const std::string& filename)
{
    std::ifstream file(filename, std::ios::ate | std::ios::binary);              //ate：从文件末尾开始读取 ， binary：以二进制形式读取文件，避免文本转换

    if (!file.is_open())
    {
        throw std::runtime_error("failed to open file !");
    }

    //从文件末尾开始读取：可以使用读取位置确定文件大小，并分配buffer
    size_t fileSize = (size_t)file.tellg();
    std::vector<char> buffer(fileSize);
    //回溯至文件开头，并读取所有字节
    file.seekg(0);
    file.read(buffer.data(),fileSize);
    //关闭文件并返回buffer
    file.close();
    return buffer;
}

/* 
把整个程序封装在类内实现，把Vulkan的对象存储为private成员，添加函数初始化这些成员，并在InitVulkan()中调用这些函数。
初始化完成后，进入mainLoop()中对帧进行渲染。
渲染循环结束后，在cleanup()中释放资源。
*/
class HelloTriangleApplication {
public:
    static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
        VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,             //指定message的严重性，该枚举值允许使用比较运算符来过滤指定严重级别的massage
        VkDebugUtilsMessageTypeFlagsEXT messageType,                        //指定massage类型
        const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,          //指针指向一个struct,其中包含message的详细信息，成员有pMessage(string),pObject(Array of handle object),objectCount(uint32_t)
        void* pUserData) {                                                  //回调时指定的数据，允许用户传递自己的数据

        std::cerr << "validation layer: " << pCallbackData->pMessage << std::endl;

        return VK_FALSE;
    }

    //声明结构体struct，用于保存queue families 的索引
    struct QueueFamilyIndices {
        std::optional<uint32_t> graphicsFamily;
        std::optional<uint32_t> presentFamily;

        bool isComplete() {
            return graphicsFamily.has_value() && presentFamily.has_value();
        }
    };
    //结构体，用于传递swap chain 的细节信息
    struct SwapChainSupportDetails {
        VkSurfaceCapabilitiesKHR capabilities;
        std::vector<VkSurfaceFormatKHR> formats;
        std::vector<VkPresentModeKHR> presentModes;
    };

public:
    void run() {
        initWindow();
        initVulkan();
        mainLoop();
        cleanup();
    }

private:
    void initWindow()
    {
        glfwInit(); // 初始化GLFW库

        glfwWindowHint(GLFW_CLIENT_API,GLFW_NO_API);    //关闭OpenGL的上下文
        //glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);     //禁止自动调整窗口大小，因为需要更小心的处理这个操作。
           
        m_window = glfwCreateWindow(WIDTH,HEIGHT,"Vulkan",nullptr,nullptr);    //实例化窗口对象
        glfwSetWindowUserPointer(m_window, this);
        glfwSetFramebufferSizeCallback(m_window, framebufferResizeCallback);
    }

    static void framebufferResizeCallback(GLFWwindow* m_window, int width, int height) {
        auto app = reinterpret_cast<HelloTriangleApplication*>(glfwGetWindowUserPointer(m_window));
        app->m_framebufferResized = true;
    }

    void initVulkan() 
    {
        CreateInstance();
        setupDebugMessenger();
        createSurface();
        pickPhysicalDevice();
        createLogicalDevice();
        createSwapChain();
        createImageViews();
        createRenderPass();
        createGraphicsPipeline();
        createFramebuffers();
        createCommandPool();
        createCommandBuffer();
        createSyncObjects();
    }

    void mainLoop() {
        //基本渲染循环
        while (!glfwWindowShouldClose(m_window))
        {
            glfwPollEvents();
            drawFrame();
        }
        vkDeviceWaitIdle(m_device);
    }

    void cleanupSwapChain() {
        for (auto framebuffer : m_swapChainFramebuffers) {
            vkDestroyFramebuffer(m_device, framebuffer, nullptr);
        }

        for (auto imageView : m_swapChainImageViews) {
            vkDestroyImageView(m_device, imageView, nullptr);
        }

        vkDestroySwapchainKHR(m_device, m_swapChain, nullptr);
    }

    void cleanup() {
        if (enableValidationLayers) {
            DestroyDebugUtilsMessengerEXT(m_instance, m_debugMessenger, nullptr);
        }
        
        cleanupSwapChain();

        for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
            vkDestroySemaphore(m_device, m_renderFinishedSemaphores[i], nullptr);
            vkDestroySemaphore(m_device, m_imageAvailableSemaphores[i], nullptr);
            vkDestroyFence(m_device, m_inFlightFences[i], nullptr);
        }

        vkDestroyCommandPool(m_device, m_commandPool, nullptr);
   
        vkDestroyPipeline(m_device, m_graphicsPipeline, nullptr);
        vkDestroyPipelineLayout(m_device, m_pipelineLayout, nullptr);
        vkDestroyRenderPass(m_device, m_renderPass, nullptr);

        vkDestroyDevice(m_device, nullptr);
        vkDestroySurfaceKHR(m_instance, m_surface, nullptr);
        vkDestroyInstance(m_instance,nullptr);
        glfwDestroyWindow(m_window);
        glfwTerminate();
    }

    void CreateInstance()
    {
        //如果需要开启validationLayer，并且检查失败，则报错
        if (enableValidationLayers && !checkValidationLayerSupport()) {
            throw std::runtime_error("validation layers requested, but not avaliable !!!");
        }

        VkApplicationInfo appInfo{};                    //使用默认值填充VkApplicationInfo结构体中的成员变量
        appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
        appInfo.pApplicationName = "Hello Triangle";
        appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
        appInfo.pEngineName = "No Engine";
        appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
        appInfo.apiVersion = VK_API_VERSION_1_0;

        //创建另一个struct对象，来描述创建Instance所需更多信息
        VkInstanceCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
        createInfo.pApplicationInfo = &appInfo;
        //配置pNext指针指向DebugMessenger扩展的Info
        VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo{};
        
        //获取需要启动的extension信息
        auto extensions = getRequiredExtensions();
        createInfo.enabledExtensionCount = static_cast<uint32_t> (extensions.size());
        createInfo.ppEnabledExtensionNames = extensions.data();
        //检查是否启用验证层 - layer
        if (enableValidationLayers) {
            createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
            createInfo.ppEnabledLayerNames = validationLayers.data();
            //配置pNext指针指向DebugMessenger扩展的Info
            populateDebugMessengerCreateInfo(debugCreateInfo);
            createInfo.pNext = (VkDebugUtilsMessengerCreateInfoEXT*)&debugCreateInfo;
        }
        else {
            createInfo.enabledLayerCount = 0;
            createInfo.pNext = nullptr;
        }

        std::cout << "required extensions:\n";
        for (const char* extension : extensions) {
            std::cout << '\t' << extension << '\n';
        }

        //---------------------- 这部分可以不需要，因为这将交由验证层来检查 ----------------------
        //获取当前SDK的扩展支持列表
        /*uint32_t ExtensionCount = 0;
        vkEnumerateInstanceExtensionProperties(nullptr, &ExtensionCount, nullptr);
        std::vector<VkExtensionProperties> Extensions(ExtensionCount);
        vkEnumerateInstanceExtensionProperties(nullptr, &ExtensionCount, Extensions.data());

        std::cout << "available extensions:\n";

        for (const auto& Extension : Extensions) {
            std::cout << '\t' << Extension.extensionName << '\n';
        }*/

        /* Challenge:检查是否所有glfw的扩展信息都在可支持的扩展列表中 */
        //数据类型分析： glfwExtension的数据类型的const char**, Extensions中的元素有成员变量 char[]，可以进行比较
        //checkExtensions(Extensions, glfwExtensions, glfwExtensionCount);
        //checkExtensions(Extensions,extensions);

        //创建Instance，并保存在成员变量中
        if (vkCreateInstance(&createInfo, nullptr, &m_instance) != VK_SUCCESS)
        {
            throw std::runtime_error("failed to create instance !!");
        }
    }

    

    void setupDebugMessenger() {
        if (!enableValidationLayers)
            return;

        VkDebugUtilsMessengerCreateInfoEXT createInfo;
        populateDebugMessengerCreateInfo(createInfo);

        if (CreateDebugUtilsMessengerEXT(m_instance, &createInfo, nullptr, &m_debugMessenger) != VK_SUCCESS) {
            throw std::runtime_error("failed to set up debug messenger!");
        }
    }

    //创建surface
    void createSurface() {
        if (glfwCreateWindowSurface(m_instance, m_window, nullptr, &m_surface) != VK_SUCCESS) {
            throw std::runtime_error("failed to create window surface!");
        }
    }
    
    //选择物理设备
    void pickPhysicalDevice() {
        //监听图形卡,查询本设备中符合Instance要求的图形卡的数量
        uint32_t deviceCount = 0;
        vkEnumeratePhysicalDevices(m_instance,&deviceCount,nullptr);
        //若符合图形卡的数量 == 0，抛出异常
        if (deviceCount == 0) {
            throw std::runtime_error("failed to find GPUs with Vulkan support!");
        }
        //分配数组保存所有的图形卡的handle
        std::vector<VkPhysicalDevice> devices(deviceCount);
        vkEnumeratePhysicalDevices(m_instance,&deviceCount,devices.data());
        for (const auto &device:devices) {
            if (isDeviceSuitable(device)) {
                m_physicalDevice = device;
                break;
            }
        }
        if (m_physicalDevice == VK_NULL_HANDLE) {
            throw std::runtime_error("failed to find a suitable GPU!");
        }
    }

    //创建逻辑设备
    void createLogicalDevice() {
        //获取queue famliy的索引
        QueueFamilyIndices indices = findQueueFamilies(m_physicalDevice);

        std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
        std::set<uint32_t> uniqueQueueFamilies = { indices.graphicsFamily.value(),indices.presentFamily.value() };
        //指定queue的优先级
        float queuePriority = 1.0f;

        for (uint32_t queueFamily : uniqueQueueFamilies) {
            //填充struct用于指定创建queue的对应信息
            VkDeviceQueueCreateInfo queueCreateInfo{};
            queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
            queueCreateInfo.queueFamilyIndex = queueFamily;
            queueCreateInfo.queueCount = 1;
            queueCreateInfo.pQueuePriorities = &queuePriority;
            queueCreateInfos.push_back(queueCreateInfo);
        }

        //指定设备支持的feature，当前默认全部都是false；
        VkPhysicalDeviceFeatures deviceFeatures{};

        //创建逻辑设备
        //建立Info - structure
        VkDeviceCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
        //添加queue Info的指针
        createInfo.pQueueCreateInfos = queueCreateInfos.data();
        createInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
        //添加feature Info的指针
        createInfo.pEnabledFeatures = &deviceFeatures;

        /* 由于最新版本的Vulkan不再区分device和instance的extension和validation layers，所以deviceCreateInfo的相关字段会被弃用 */
        //设置device扩展
        createInfo.enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size());
        createInfo.ppEnabledExtensionNames = deviceExtensions.data();
        //向device中添加验证层
        if (enableValidationLayers) {
            createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
            createInfo.ppEnabledLayerNames = validationLayers.data();
        }
        else {
            createInfo.enabledLayerCount = 0;
        }

        //实例化逻辑设备对象
        if (vkCreateDevice(m_physicalDevice, &createInfo, nullptr, &m_device) != VK_SUCCESS) {
            throw std::runtime_error("failed to create logical device!");
        }

        vkGetDeviceQueue(m_device, indices.graphicsFamily.value(), 0, &m_graphicsQueue);
        vkGetDeviceQueue(m_device, indices.presentFamily.value(), 0, &m_presentQueue);
    }

    void createSwapChain()
    {
        //请求 device 的 swapchain 支持 (details)
        SwapChainSupportDetails swapChainSupport = querySwapChainSupport(m_physicalDevice);
        //从 details 中选择最合适的 swapchain 配置
        VkSurfaceFormatKHR surfaceFormat = chooseSwapSurfaceFormat(swapChainSupport.formats);
        VkPresentModeKHR presentMode = chooseSwapPresentMode(swapChainSupport.presentModes);
        VkExtent2D extent = chooseSwapExtent(swapChainSupport.capabilities);
        uint32_t imageCount = swapChainSupport.capabilities.minImageCount + 1;  //配置 swapchain 中的图像数量
        // maxImageCount == 0 代表没有最大值约束
        // 表示在有最大值的情况下，若当前值 > 最大值，则将imageCount设置为更小的值
        if (swapChainSupport.capabilities.maxImageCount > 0 && imageCount > swapChainSupport.capabilities.maxImageCount) {
            imageCount = swapChainSupport.capabilities.maxImageCount;
        }

        VkSwapchainCreateInfoKHR createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
        createInfo.surface = m_surface;

        createInfo.minImageCount = imageCount;
        createInfo.imageFormat = surfaceFormat.format;
        createInfo.imageColorSpace = surfaceFormat.colorSpace;
        createInfo.imageExtent = extent;
        createInfo.imageArrayLayers = 1;                                        //指定每个图像包含的层数，
        createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;            //定义图像的用途，Color Attachment 颜色附件
                                                                                //若要执行后处理操作，可以将其指定为TRANSFER_DST_BIT,并通过内存操作在渲染图像和swapchain图像之间交换数据

        //获取并设置处理swapchain中image使用的物理设备的queue family信息
        QueueFamilyIndices indices = findQueueFamilies(m_physicalDevice);
        uint32_t queueFamilyIndices[] = { indices.graphicsFamily.value(), indices.presentFamily.value() };

        //若graphics 和 presentation 使用不同的 queue 处理
        //swapchain 中，我们希望在 graphics 队列中渲染image ， 然后在 presentation 队列中提交 image 
        if (indices.graphicsFamily != indices.presentFamily) {
            createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;   //并发模式， 从多个queue访问的图像，处理模式为Concurrent，无需显式的所有权转移
            createInfo.queueFamilyIndexCount = 2;                       //使用参数指定：有哪些queue共享image所有权
            createInfo.pQueueFamilyIndices = queueFamilyIndices;
        }
        else {
            createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;    //独占模式， 不同的queue访问图像时，必须显式的修改image的所有权
            createInfo.queueFamilyIndexCount = 0; // Optional
            createInfo.pQueueFamilyIndices = nullptr; // Optional
        }

        //设置pretransform，如：水平翻转
        createInfo.preTransform = swapChainSupport.capabilities.currentTransform;   //使用currentTransform，设置为无pretransform

        //设置是否使用alpha通道与其他窗口进行混合
        createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;              //希望忽略alpha通道

        //clipped设置为VK_TRUE，代表不关心被其他window遮挡的像素是什么颜色
        createInfo.presentMode = presentMode;
        createInfo.clipped = VK_TRUE;                                               //启用裁剪来获得最佳性能

        //用于处理swapchain失效的情况，如：window大小调整
        createInfo.oldSwapchain = VK_NULL_HANDLE;                                   //暂时不做处理，以后讨论

        //创建swapchain
        if (vkCreateSwapchainKHR(m_device, &createInfo, nullptr, &m_swapChain) != VK_SUCCESS) {
            throw std::runtime_error("failed to create swap chain!");
        }

        //检索swapchain中的图像，并保存相关引用
        vkGetSwapchainImagesKHR(m_device, m_swapChain, &imageCount, nullptr);
        m_swapChainImages.resize(imageCount);
        vkGetSwapchainImagesKHR(m_device, m_swapChain, &imageCount, m_swapChainImages.data());

        //保存成员变量format 和 extent
        m_swapChainImageFormat = surfaceFormat.format;
        m_swapChainExtent = extent;
    }

    //为Swapchain中的每个image创建ImageView
    void createImageViews() {
        m_swapChainImageViews.resize(m_swapChainImages.size());

        for (size_t i = 0; i < m_swapChainImages.size(); i++) {
            VkImageViewCreateInfo createInfo{};
            createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;        //类型
            createInfo.image = m_swapChainImages[i];                            //数据
            createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;                        //1D \ 2D \ 3D Texture or CubeMap
            createInfo.format = m_swapChainImageFormat;                         //32bit RGBA
            
            createInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;            //component用于混合颜色通道
            createInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
            createInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
            createInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;

            createInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT; //subresourceRange用于描述图像用途，以及哪一部分可以被访问
            createInfo.subresourceRange.baseMipLevel = 0;                       //创建没有任何mipmap和layer的 color target
            createInfo.subresourceRange.levelCount = 1;
            createInfo.subresourceRange.baseArrayLayer = 0;
            createInfo.subresourceRange.layerCount = 1;

            //调用create API
            if (vkCreateImageView(m_device, &createInfo, nullptr, &m_swapChainImageViews[i]) != VK_SUCCESS) {
                throw std::runtime_error("failed to create image views!");
            }

        }
    }

    //创建render pass
    void createRenderPass()
    {
        VkAttachmentDescription colorAttachment{};                                          //attachment说明：描述attachment的各种属性
        colorAttachment.format = m_swapChainImageFormat;
        colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
        colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;                               //渲染前，attachment中的数据如何处理
        colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;                             //渲染后，attachment中的数据如何处理：渲染后保存在内存中，之后可以读取里面的数据
        colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;                    //暂时不关心模板数据
        colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;                          //pixel在内存中的layout可以被修改：不关心render pass开始之前的image layout是什么
        colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;                      //希望render pass 的输出是将在交换链中被present的image的layout，此外：作为color attachment的layout、用作内存复制操作目标的layout （提问：三者的内存layout有何不同）
        
        //subpass 和 attachment reference
        VkAttachmentReference colorAttachmentRef{};
        colorAttachmentRef.attachment = 0;                                                  //attachment array‘s index
        colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        VkSubpassDescription subpass{};
        subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpass.colorAttachmentCount = 1;
        subpass.pColorAttachments = &colorAttachmentRef;                                    //通过 layout(location = 0) out vec4 outColor,可以在 fragment shader中直接引用这个attachment

        //设置subpass dependency属性
        VkSubpassDependency dependency{};
        dependency.srcSubpass = VK_SUBPASS_EXTERNAL;                                        //render pass之前的隐式子通道转换
        dependency.dstSubpass = 0;                                                          //subpass的索引，本例中有且只有一个subpass
        dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;            //指定要等待的操作和操作发生的阶段。这里指明等待color attachment output阶段
        dependency.srcAccessMask = 0;
        dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;            //要等待的阶段是color attachment
        dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;                    //要等待的操作是 color attachment 的写入操作
        /* 这些操作将阻止转化发生，直至想向其写入颜色之前 */


        //创建render pass 对象
        VkRenderPassCreateInfo renderPassInfo{};
        renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        renderPassInfo.attachmentCount = 1;
        renderPassInfo.pAttachments = &colorAttachment;
        renderPassInfo.subpassCount = 1;
        renderPassInfo.pSubpasses = &subpass;
        renderPassInfo.dependencyCount = 1;
        renderPassInfo.pDependencies = &dependency;

        if (vkCreateRenderPass(m_device, &renderPassInfo, nullptr, &m_renderPass) != VK_SUCCESS) {
            throw std::runtime_error("failed to create render pass!");
        }
    }
    
    //创建render pipeline
    void createGraphicsPipeline()
    {
        //加载shader
        std::vector<char> vertShaderCode = readFile("shaders/vert.spv");
        std::vector<char> fragShaderCode = readFile("shaders/frag.spv");
        //创建shaderModule
        VkShaderModule vertShaderModule = createShaderModule(vertShaderCode);
        VkShaderModule fragShaderModule = createShaderModule(fragShaderCode);

        /* ... */
        //创建vertex shader的 stage info
        VkPipelineShaderStageCreateInfo vertShaderStageInfo{};
        vertShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        vertShaderStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;                             //在pipeline的哪个阶段使用shader
        vertShaderStageInfo.module = vertShaderModule;
        vertShaderStageInfo.pName = "main";                                                 //要调用的函数，shader的入口点
        //创建fragment shader 的 stage info
        VkPipelineShaderStageCreateInfo fragShaderStageInfo{};
        fragShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        fragShaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
        fragShaderStageInfo.module = fragShaderModule;
        fragShaderStageInfo.pName = "main";

        VkPipelineShaderStageCreateInfo shaderStages[] = { vertShaderStageInfo, fragShaderStageInfo };

        //创建 dynamic states 的 create info
        /*std::vector<VkDynamicState> dynamicStates = {
            VK_DYNAMIC_STATE_VIEWPORT,
            VK_DYNAMIC_STATE_SCISSOR
        };

        VkPipelineDynamicStateCreateInfo dynamicState{};
        dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
        dynamicState.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
        dynamicState.pDynamicStates = dynamicStates.data();*/

        //创建 vertex input stage 的 create info
        VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
        vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
        vertexInputInfo.vertexBindingDescriptionCount = 0;
        vertexInputInfo.pVertexBindingDescriptions = nullptr; // Optional
        vertexInputInfo.vertexAttributeDescriptionCount = 0;
        vertexInputInfo.pVertexAttributeDescriptions = nullptr; // Optional

        //创建 input assembly stage 的 create info
        VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
        inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
        inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        inputAssembly.primitiveRestartEnable = VK_FALSE;

        //定义 视口Viewport 和 裁剪scissors
        //Viewport
        VkViewport viewport{};
        viewport.x = 0.0f;
        viewport.y = 0.0f;
        viewport.width = (float)m_swapChainExtent.width;                  //交换链及其图像的大小可能与window的尺寸不同
        viewport.height = (float)m_swapChainExtent.height;                //由于交换链中的图像将被用作framebuffer，所以这里使viewport与交换链中的image大小一致
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;
        //scissor
        VkRect2D scissor{};
        scissor.offset = { 0, 0 };
        scissor.extent = m_swapChainExtent;
        //设置viewport 和 scissor 为动态state
        std::vector<VkDynamicState> dynamicStates = {
            VK_DYNAMIC_STATE_VIEWPORT,
            VK_DYNAMIC_STATE_SCISSOR
        };

        VkPipelineDynamicStateCreateInfo dynamicState{};
        dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
        dynamicState.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
        dynamicState.pDynamicStates = dynamicStates.data();
        //创建viewport 和 scissor 的 create info
        VkPipelineViewportStateCreateInfo viewportState{};
        viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
        viewportState.viewportCount = 1;
        viewportState.scissorCount = 1;
        //viewportState.pViewports = &viewport;
        //viewportState.pScissors = &scissor;

        //光栅化 state 的 create info
        VkPipelineRasterizationStateCreateInfo rasterizer{};
        rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
        rasterizer.depthClampEnable = VK_FALSE;                                                     //若启用，则超过近平面和远平面的fragment将被clip，在渲染shadow map时将会很有用
        rasterizer.rasterizerDiscardEnable = VK_FALSE;                                              //若启用，几何体基本上不会通过光栅化阶段，这基本上就禁止了向framebuffer的任何输出
        rasterizer.polygonMode = VK_POLYGON_MODE_FILL;                                              //选择模式为填充模式，除此之外还有 线框绘制模式 还有 点绘制模式
        rasterizer.lineWidth = 1.0f;                                                                //定义线宽
        rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;                                                //面剔除的类型，前向剔除 背向剔除 以及 全部剔除
        rasterizer.frontFace = VK_FRONT_FACE_CLOCKWISE;                                             //前向面是顺时针顶点顺序还是逆时针
        rasterizer.depthBiasEnable = VK_FALSE;                                                      //是否启用深度修正，可通过添加常量 或 根据fragment的斜率对depth进行偏置
        rasterizer.depthBiasConstantFactor = 0.0f; // Optional
        rasterizer.depthBiasClamp = 0.0f; // Optional
        rasterizer.depthBiasSlopeFactor = 0.0f; // Optional

        //多重采样 state 的 create info
        VkPipelineMultisampleStateCreateInfo multisampling{};
        multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
        multisampling.sampleShadingEnable = VK_FALSE;                                               //暂时关闭mutisampling
        multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
        multisampling.minSampleShading = 1.0f; // Optional
        multisampling.pSampleMask = nullptr; // Optional
        multisampling.alphaToCoverageEnable = VK_FALSE; // Optional
        multisampling.alphaToOneEnable = VK_FALSE; // Optional

        //深度和模板测试 state 的 create info
        //暂时不使用深度和模板测试

        //颜色混合 state 的 create info
        VkPipelineColorBlendAttachmentState colorBlendAttachment{};                                 //设置一个framebuffer的attachment
        colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
        colorBlendAttachment.blendEnable = VK_TRUE;                                                 //若禁用，来自fragment shader 的 color 将不加修改的覆盖framebuffer
        colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;                       //当前混合模式设置为 alpha 混合
        colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
        colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
        colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
        colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;

        VkPipelineColorBlendStateCreateInfo colorBlending{};                                        //将引用所有framebuffer的struct数组，配置全局设置
        colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        colorBlending.logicOpEnable = VK_FALSE;                                                     //是否启用按位混合运算方式，会自动禁用第一种混合模式
        colorBlending.logicOp = VK_LOGIC_OP_COPY; // Optional
        colorBlending.attachmentCount = 1;
        colorBlending.pAttachments = &colorBlendAttachment;
        colorBlending.blendConstants[0] = 0.0f; // Optional
        colorBlending.blendConstants[1] = 0.0f; // Optional
        colorBlending.blendConstants[2] = 0.0f; // Optional
        colorBlending.blendConstants[3] = 0.0f; // Optional

        //创建 pipeline layout 用于传递 uniform 变量
        VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
        pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        pipelineLayoutInfo.setLayoutCount = 0; // Optional
        pipelineLayoutInfo.pSetLayouts = nullptr; // Optional
        pipelineLayoutInfo.pushConstantRangeCount = 0; // Optional
        pipelineLayoutInfo.pPushConstantRanges = nullptr; // Optional

        if (vkCreatePipelineLayout(m_device, &pipelineLayoutInfo, nullptr, &m_pipelineLayout) != VK_SUCCESS) {
            throw std::runtime_error("failed to create pipeline layout!");
        }


        //Combine to create graphics pipeline
        VkGraphicsPipelineCreateInfo pipelineInfo{};
        pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
        //shader stages information
        pipelineInfo.stageCount = 2;
        pipelineInfo.pStages = shaderStages;
        //dynamic functions information
        pipelineInfo.pVertexInputState = &vertexInputInfo;
        pipelineInfo.pInputAssemblyState = &inputAssembly;
        pipelineInfo.pViewportState = &viewportState;
        pipelineInfo.pRasterizationState = &rasterizer;
        pipelineInfo.pMultisampleState = &multisampling;
        pipelineInfo.pDepthStencilState = nullptr; // Optional
        pipelineInfo.pColorBlendState = &colorBlending;
        pipelineInfo.pDynamicState = &dynamicState;
        //pipeline layout information
        pipelineInfo.layout = m_pipelineLayout;
        //render passes information
        pipelineInfo.renderPass = m_renderPass;
        pipelineInfo.subpass = 0;                       // 使用 render pass 的 subpass index
        //pipeline 继承相关
        pipelineInfo.basePipelineHandle = VK_NULL_HANDLE; // Optional
        pipelineInfo.basePipelineIndex = -1; // Optional

        if (vkCreateGraphicsPipelines(m_device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &m_graphicsPipeline) != VK_SUCCESS) {
            throw std::runtime_error("failed to create graphics pipeline!");
        }
        
        //销毁shaderModule
        vkDestroyShaderModule(m_device, fragShaderModule, nullptr);
        vkDestroyShaderModule(m_device, vertShaderModule, nullptr);
    }

    VkShaderModule createShaderModule(const std::vector<char> &code)
    {
        VkShaderModuleCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        createInfo.codeSize = code.size();
        createInfo.pCode = reinterpret_cast<const uint32_t*>(code.data());

        VkShaderModule shaderModule;
        if (vkCreateShaderModule(m_device, &createInfo, nullptr, &shaderModule) != VK_SUCCESS) {
            throw std::runtime_error("failed to create shader module!");
        }

        return shaderModule;
    }

    void createFramebuffers() {
        m_swapChainFramebuffers.resize(m_swapChainImageViews.size());

        for (size_t i = 0; i < m_swapChainImageViews.size(); i++) {
            VkImageView attachments[] = {
                m_swapChainImageViews[i]
            };

            VkFramebufferCreateInfo framebufferInfo{};
            framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
            framebufferInfo.renderPass = m_renderPass;
            framebufferInfo.attachmentCount = 1;                                        //Imageview 对象的数量。需要绑定至render pass中与之对应的attachment description
            framebufferInfo.pAttachments = attachments;                                 //Imageview 对象数组的指针
            framebufferInfo.width = m_swapChainExtent.width;
            framebufferInfo.height = m_swapChainExtent.height;
            framebufferInfo.layers = 1;

            if (vkCreateFramebuffer(m_device, &framebufferInfo, nullptr, &m_swapChainFramebuffers[i]) != VK_SUCCESS) {
                throw std::runtime_error("failed to create framebuffer!");
            }
        }
    }

    void createCommandPool()
    {
        QueueFamilyIndices queueFamilyIndices = findQueueFamilies(m_physicalDevice);

        VkCommandPoolCreateInfo poolInfo{};
        poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;               //允许command buffer单独重新记录，否则所有command buffer必须一起重制
        poolInfo.queueFamilyIndex = queueFamilyIndices.graphicsFamily.value();

        if (vkCreateCommandPool(m_device, &poolInfo, nullptr, &m_commandPool) != VK_SUCCESS) {
            throw std::runtime_error("failed to create command pool!");
        }
    }

    void createCommandBuffer()
    {
        m_commandBuffers.resize(MAX_FRAMES_IN_FLIGHT);

        VkCommandBufferAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocInfo.commandPool = m_commandPool;
        allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;                              //分配的command buffer是主buffer 还是辅助buffer。primary：可以提交到队列中执行，但不能调用其他command buffers。secondary：不能被直接提交，但可以调用其他command buffers
        allocInfo.commandBufferCount = (uint32_t)m_commandBuffers.size();

        if (vkAllocateCommandBuffers(m_device, &allocInfo, m_commandBuffers.data()) != VK_SUCCESS) {
            throw std::runtime_error("failed to allocate command buffers!");
        }
    }

    void createSyncObjects() {
        m_imageAvailableSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
        m_renderFinishedSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
        m_inFlightFences.resize(MAX_FRAMES_IN_FLIGHT);

        VkSemaphoreCreateInfo semaphoreInfo{};
        semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

        VkFenceCreateInfo fenceInfo{};
        fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

        for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
            if (vkCreateSemaphore(m_device, &semaphoreInfo, nullptr, &m_imageAvailableSemaphores[i]) != VK_SUCCESS ||
                vkCreateSemaphore(m_device, &semaphoreInfo, nullptr, &m_renderFinishedSemaphores[i]) != VK_SUCCESS ||
                vkCreateFence(m_device, &fenceInfo, nullptr, &m_inFlightFences[i]) != VK_SUCCESS) {

                throw std::runtime_error("failed to create synchronization objects for a frame!");
            }
        }
    }



    void recreateSwapChain()
    {
        int width = 0, height = 0;
        glfwGetFramebufferSize(m_window, &width, &height);
        while (width == 0 || height == 0) {
            glfwGetFramebufferSize(m_window, &width, &height);
            glfwWaitEvents();
        }

        vkDeviceWaitIdle(m_device);

        cleanupSwapChain();

        createSwapChain();
        createImageViews();
        createFramebuffers();
    }
    /* -------------------------------------------------------------------- tool function ---------------------------------------------------------------------------------------- */
    private:

    bool checkValidationLayerSupport() {
        //首先列举所有可获得的layer
        uint32_t layerCount;
        vkEnumerateInstanceLayerProperties(&layerCount, nullptr);

        std::vector<VkLayerProperties> avaliableLayers(layerCount);
        vkEnumerateInstanceLayerProperties(&layerCount, avaliableLayers.data());

        //检查所有validationLayer中的层是否在avaliableLayers中存在
        for (const char* layerName : validationLayers) {
            bool layerFound = false;

            for (const auto& layerProperties : avaliableLayers) {
                if (strcmp(layerName, layerProperties.layerName) == 0) {
                    layerFound = true;
                    break;
                }
            }

            if (!layerFound) {
                return false;
            }
        }

        return true;
    }

    std::vector<const char*> getRequiredExtensions()
    {
        uint32_t glfwExtensionCount = 0;
        const char** glfwExtensions;
        glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

        std::vector<const char*> extensions(glfwExtensions, glfwExtensions + glfwExtensionCount); //使用指针地址做赋值，指定了一个指针区间
        if (enableValidationLayers) {
            extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);    //使用宏代替字符串，避免拼写错误
        }
        return extensions;
    }

    void checkExtensions(const std::vector<VkExtensionProperties>& Extensions,
        const std::vector<const char*>& extensions)
    {
        for (const char* extension : extensions) {
            bool if_find = false;
            for (const auto& Extension : Extensions) {
                if (strcmp(extension, Extension.extensionName) == 0) {
                    if_find = true;
                    break;
                }
            }
            if (!if_find) {
                throw std::runtime_error(std::string(extension) + std::string(" is not find ...."));
            }
        }
        std::cout << "Extensions checking has Success ... \n";
    }

    //检查设备扩展支持
    bool checkDeviceExtensionSupport(VkPhysicalDevice device)
    {
        uint32_t extensionCount;
        vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr);

        std::vector<VkExtensionProperties> avaliableExtensions(extensionCount);
        vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, avaliableExtensions.data());

        std::set<std::string> requiredExtensions(deviceExtensions.begin(), deviceExtensions.end());

        for (const auto& extension : avaliableExtensions)
        {
            requiredExtensions.erase(extension.extensionName);
        }

        return requiredExtensions.empty();
    };

    /* 把DebugMessenger创建时所需的Info结构体的配置提取到这个函数中 */
    void populateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& createInfo) {
        createInfo = {};
        createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
        createInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
        createInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
        createInfo.pfnUserCallback = debugCallback;
    }

    //检查图形卡是否符合我们的要求
    bool isDeviceSuitable(VkPhysicalDevice device) {
        //获取设备属性
        VkPhysicalDeviceProperties deviceProperties;
        vkGetPhysicalDeviceProperties(device, &deviceProperties);
        //获取设备可选择的feature
        VkPhysicalDeviceFeatures deviceFeatures;
        vkGetPhysicalDeviceFeatures(device, &deviceFeatures);
        //可以从设备中获取更多信息 ........

        //检查设备是否支持 指定类型的队列family
        QueueFamilyIndices indices = findQueueFamilies(device);

        //检查设备是否支持 指定的设备扩展
        bool extensionsSupported = checkDeviceExtensionSupport(device);

        //检查设备和surface是否支持 swap chain 的使用detail
        bool swapChainAdequate = false;
        if (extensionsSupported) {
            SwapChainSupportDetails swapChainSupport = querySwapChainSupport(device);
            swapChainAdequate = !swapChainSupport.formats.empty() && !swapChainSupport.presentModes.empty();
        }

        //希望设备类型为独立显卡，并且支持geometryShader
        return deviceProperties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU &&
            indices.isComplete() &&
            extensionsSupported &&
            swapChainAdequate;
    }
    
    //查找设备中是否我们需要的Queue families
    QueueFamilyIndices findQueueFamilies(VkPhysicalDevice device) {
        QueueFamilyIndices indices;
        // Assign index to queue families that could be found
        uint32_t queueFamilyCount = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);

        std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
        vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies.data());

        //找到至少一个支持VK_QUEUE_GRAPHICS_BIT的family
        int i = 0;
        for (const auto& queueFamily : queueFamilies) {

            if (queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT) {
                indices.graphicsFamily = i;
            }

            VkBool32 presentSupport = false;
            vkGetPhysicalDeviceSurfaceSupportKHR(device, i, m_surface, &presentSupport);
            if (presentSupport) {
                indices.presentFamily = i;
            }

            if (indices.isComplete()) {
                break;
            }

            i++;
        }

        return indices;
    }

    //检查device对swap chain的支持细节
    SwapChainSupportDetails querySwapChainSupport(VkPhysicalDevice device) {
        SwapChainSupportDetails details;
        //查询Capbilities属性       physical device 和 surface 是 swapchain 最核心的两个组件
        vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, m_surface, &details.capabilities);
        //查询Surface format 属性
        uint32_t formatCount;
        vkGetPhysicalDeviceSurfaceFormatsKHR(device, m_surface, &formatCount, nullptr);
        if (formatCount != 0) {
            details.formats.resize(formatCount);
            vkGetPhysicalDeviceSurfaceFormatsKHR(device, m_surface, &formatCount, details.formats.data());
        }
        //查询Surface presentation mode属性
        uint32_t presentModeCount;
        vkGetPhysicalDeviceSurfacePresentModesKHR(device, m_surface, &presentModeCount, nullptr);
        if (presentModeCount != 0) {
            details.presentModes.resize(presentModeCount);
            vkGetPhysicalDeviceSurfacePresentModesKHR(device, m_surface, &presentModeCount, details.presentModes.data());
        }


        return details;
    }

    //选择Swap chain 的 SurfaceFormat                  输入vector是 details中的formats成员变量
    VkSurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats) {
        //每个VkSurfaceFormatKHR包含两个成员：format 和 colorspace
        //format描述color的通道和类型
        //colorspace使用VK_COLOR_SPACE_SRGB_NONLINEAR_KHR指示是否支持SRGB颜色空间

        //期望：使用SRGB 颜色空间 和 32bit颜色格式 
        for (const VkSurfaceFormatKHR& availableFormat : availableFormats) {
            if (availableFormat.format == VK_FORMAT_B8G8R8A8_SRGB && availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
                return availableFormat;
            }
        }

        //若期望format不存在，则返回第一个位置的format
        return availableFormats[0];
    }

    //选择Swap chain 的 presentation mode
    VkPresentModeKHR chooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes) {
        for (const auto& availablePresentMode : availablePresentModes) {
            if (availablePresentMode == VK_PRESENT_MODE_MAILBOX_KHR) {
                return availablePresentMode;
            }
        }
        return VK_PRESENT_MODE_FIFO_KHR;
    }

    //选择Swap chain 的 Swap extent ， 分辨率  通常与窗口分辨率匹配
    VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities) {
        //从SurfaceCapbilities中获取当前窗口的分辨率
        if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max()) {
            return capabilities.currentExtent;
        }
        else { //自适应调节分辨率
            int width, height;
            glfwGetFramebufferSize(m_window,&width,&height);

            VkExtent2D actualExtent = {
                static_cast<uint32_t>(width),
                static_cast<uint32_t>(height)
            };
            //clamp函数用于把实际的分辨率约束至device和surface允许的范围
            actualExtent.width = std::clamp(actualExtent.width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
            actualExtent.height = std::clamp(actualExtent.height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);

            return actualExtent;
        }
    }

    void recordCommandBuffer(VkCommandBuffer commandBuffer, uint32_t imageIndex) {
        VkCommandBufferBeginInfo beginInfo{};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.flags = 0; // Optional                                                                //指定如何使用command buffer：执行一次后立即重新记录、定义为辅助缓冲区并完全位于renderpass中、可以重新提交并正等待执行
        beginInfo.pInheritanceInfo = nullptr; // Optional

        if (vkBeginCommandBuffer(commandBuffer, &beginInfo) != VK_SUCCESS) {
            throw std::runtime_error("failed to begin recording command buffer!");
        }

        VkRenderPassBeginInfo renderPassInfo{};
        renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        renderPassInfo.renderPass = m_renderPass;                                                       //当前使用的render pass
        renderPassInfo.framebuffer = m_swapChainFramebuffers[imageIndex];                               //swap chain中等待绘制的image对应的attachment绑定的framebuffer
        renderPassInfo.renderArea.offset = { 0, 0 };                                                    //shader的加载和保存从什么位置开始
        renderPassInfo.renderArea.extent = m_swapChainExtent;                                             //shader的加载和保存的范围
        VkClearValue clearColor = { {{0.0f, 0.0f, 0.0f, 1.0f}} };                                       //用于attachment在加载时清除操作的清除值
        renderPassInfo.clearValueCount = 1;
        renderPassInfo.pClearValues = &clearColor;
        //render pass 开始！！
        vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);               //第一个参数：需要被记录命令的command buffer，第二个参数：指定的render pass细节，第三个参数：如何提供render pass中的绘图指令

        //绑定pipline
        vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_graphicsPipeline);          //第二个参数：graphics pipeline还是compute pipeline
        //由于定义了viewport和scissors是动态的，这里需要显式设置他们
        VkViewport viewport{};
        viewport.x = 0.0f;
        viewport.y = 0.0f;
        viewport.width = static_cast<float>(m_swapChainExtent.width);
        viewport.height = static_cast<float>(m_swapChainExtent.height);
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;
        vkCmdSetViewport(commandBuffer, 0, 1, &viewport);

        VkRect2D scissor{};
        scissor.offset = { 0, 0 };
        scissor.extent = m_swapChainExtent;
        vkCmdSetScissor(commandBuffer, 0, 1, &scissor);


        //提交三角形的绘制指令
        vkCmdDraw(commandBuffer, 3, 1, 0, 0);                                                           //从第二个参数开始分别是：vertexCount\instanceCount\firstVertex(offset of VertexBuffer)\firstInstance(offset of Instance Draw)

        //收尾
        vkCmdEndRenderPass(commandBuffer);
        if (vkEndCommandBuffer(commandBuffer) != VK_SUCCESS) {
            throw std::runtime_error("failed to record command buffer!");
        }
    }

private:
    void drawFrame()
    {
        vkWaitForFences(m_device, 1, &m_inFlightFences[m_currentFrame], VK_TRUE, UINT64_MAX);                            //使用VK_TRUE作为参数是确保Fence数组中所有Fence都有信号才继续执行.  UINT64_MAX是超时参数

        uint32_t imageIndex;
        //第三个参数：超时控制；第四个参数：信号量；第五个参数：fence；第六个参数：用于接受image索引的整型变量
        VkResult result = vkAcquireNextImageKHR(m_device, m_swapChain, UINT64_MAX, m_imageAvailableSemaphores[m_currentFrame], VK_NULL_HANDLE, &imageIndex);
        if (result == VK_ERROR_OUT_OF_DATE_KHR) {
            recreateSwapChain();
            return;
        }
        else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
            throw std::runtime_error("failed to acquire swap chain image!");
        }
        vkResetFences(m_device, 1, &m_inFlightFences[m_currentFrame]);                                                   //等待后手动将Fence重置为无信号状态
        
        //重置Command buffer
        vkResetCommandBuffer(m_commandBuffers[m_currentFrame], 0);
        
        //记录Command buffer
        recordCommandBuffer(m_commandBuffers[m_currentFrame], imageIndex);                                                //调用实现好的函数，向command buffer中写入命令
        
        //提交Command buffer
        VkSubmitInfo submitInfo{};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        VkSemaphore waitSemaphores[] = { m_imageAvailableSemaphores[m_currentFrame]};                                   //等待哪个信号量：从swap chain中获取image是否结束
        VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };          //在pipeline的哪个阶段等待：color attachment output阶段...该阶段开始之前等待获取image -- 这意味着在image不可用之前可以执行vertex shader
        submitInfo.waitSemaphoreCount = 1;
        submitInfo.pWaitSemaphores = waitSemaphores;
        submitInfo.pWaitDstStageMask = waitStages;                                                      //waitStage中的每个条目的索引与waitSemaphores数组中的信号量的索引一一对应
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &m_commandBuffers[m_currentFrame];
        VkSemaphore signalSemaphores[] = { m_renderFinishedSemaphores[m_currentFrame]};                                 //指定Command buffer执行完毕后，会激活哪些信号量
        submitInfo.signalSemaphoreCount = 1;
        submitInfo.pSignalSemaphores = signalSemaphores;
        if (vkQueueSubmit(m_graphicsQueue, 1, &submitInfo, m_inFlightFences[m_currentFrame]) != VK_SUCCESS) {
            throw std::runtime_error("failed to submit draw command buffer!");
        }

        //presentation操作
        VkPresentInfoKHR presentInfo{};
        presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;

        presentInfo.waitSemaphoreCount = 1;                                                             //presentation操作要等待的信号量
        presentInfo.pWaitSemaphores = signalSemaphores;

        VkSwapchainKHR swapChains[] = { m_swapChain };
        presentInfo.swapchainCount = 1;
        presentInfo.pSwapchains = swapChains;                                                           //指定swap chain 和 swap chain中image的索引
        presentInfo.pImageIndices = &imageIndex;

        presentInfo.pResults = nullptr; // Optional                                                     //允许指定VkResult数组，来检查每个swap chain中的image是否被presentation成功

        result = vkQueuePresentKHR(m_presentQueue, &presentInfo);                                       //向presentQueue提交present命令。此时由于同步操作的影响，被present的image一定被render完成了。
        if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || m_framebufferResized) {
            m_framebufferResized = false;
            recreateSwapChain();
        }
        else if (result != VK_SUCCESS) {
            throw std::runtime_error("failed to present swap chain image!");
        }

        m_currentFrame = (m_currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
    }

private:
    GLFWwindow *m_window;
    VkInstance m_instance;
    VkDebugUtilsMessengerEXT m_debugMessenger;
    VkSurfaceKHR m_surface;
    VkPhysicalDevice m_physicalDevice = VK_NULL_HANDLE;
    VkDevice m_device;
    VkQueue m_graphicsQueue;
    VkQueue m_presentQueue;
    VkSwapchainKHR m_swapChain;
    std::vector<VkImage> m_swapChainImages;
    VkFormat m_swapChainImageFormat;
    VkExtent2D m_swapChainExtent;
    std::vector<VkImageView> m_swapChainImageViews;
    VkRenderPass m_renderPass;
    VkPipelineLayout m_pipelineLayout;
    VkPipeline m_graphicsPipeline;
    std::vector<VkFramebuffer> m_swapChainFramebuffers;
    VkCommandPool m_commandPool;
    std::vector<VkCommandBuffer> m_commandBuffers;
    //同步
    std::vector<VkSemaphore> m_imageAvailableSemaphores;
    std::vector<VkSemaphore> m_renderFinishedSemaphores;
    std::vector<VkFence> m_inFlightFences;

    uint32_t m_currentFrame = 0;
    bool m_framebufferResized = false;
};

int main() {
    HelloTriangleApplication app;

    try {
        app.run();
    }
    catch (const std::exception& e) {           // 捕捉 throw 扔出的异常， 接收数据类型需要有 throw 抛出的数据类型匹配
        std::cerr << e.what() << std::endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}