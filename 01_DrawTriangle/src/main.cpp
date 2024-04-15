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

//ָ��ȫ�ֱ�����������֤�������
const std::vector<const char*> validationLayers = {
    "VK_LAYER_KHRONOS_validation"
};

//ָ��ȫ�ֱ�����Device��չ�б�
const std::vector<const char*> deviceExtensions = {
    VK_KHR_SWAPCHAIN_EXTENSION_NAME
};

#ifdef NDEBUG   //���û������debugģʽ����validation layer������
    const bool enableValidationLayers = false;
#else
    const bool enableValidationLayers = true;
#endif

/* 
�Զ�������������ڲ��Ҳ��ᱻ�Զ����ص���չ����CreateDebugUtilsMessengerEXT �� DestroyDebugUtilsMessengerEXT
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

//�������������ļ��м��ض���������
static std::vector<char> readFile(const std::string& filename)
{
    std::ifstream file(filename, std::ios::ate | std::ios::binary);              //ate�����ļ�ĩβ��ʼ��ȡ �� binary���Զ�������ʽ��ȡ�ļ��������ı�ת��

    if (!file.is_open())
    {
        throw std::runtime_error("failed to open file !");
    }

    //���ļ�ĩβ��ʼ��ȡ������ʹ�ö�ȡλ��ȷ���ļ���С��������buffer
    size_t fileSize = (size_t)file.tellg();
    std::vector<char> buffer(fileSize);
    //�������ļ���ͷ������ȡ�����ֽ�
    file.seekg(0);
    file.read(buffer.data(),fileSize);
    //�ر��ļ�������buffer
    file.close();
    return buffer;
}

/* 
�����������װ������ʵ�֣���Vulkan�Ķ���洢Ϊprivate��Ա����Ӻ�����ʼ����Щ��Ա������InitVulkan()�е�����Щ������
��ʼ����ɺ󣬽���mainLoop()�ж�֡������Ⱦ��
��Ⱦѭ����������cleanup()���ͷ���Դ��
*/
class HelloTriangleApplication {
public:
    static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
        VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,             //ָ��message�������ԣ���ö��ֵ����ʹ�ñȽ������������ָ�����ؼ����massage
        VkDebugUtilsMessageTypeFlagsEXT messageType,                        //ָ��massage����
        const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,          //ָ��ָ��һ��struct,���а���message����ϸ��Ϣ����Ա��pMessage(string),pObject(Array of handle object),objectCount(uint32_t)
        void* pUserData) {                                                  //�ص�ʱָ�������ݣ������û������Լ�������

        std::cerr << "validation layer: " << pCallbackData->pMessage << std::endl;

        return VK_FALSE;
    }

    //�����ṹ��struct�����ڱ���queue families ������
    struct QueueFamilyIndices {
        std::optional<uint32_t> graphicsFamily;
        std::optional<uint32_t> presentFamily;

        bool isComplete() {
            return graphicsFamily.has_value() && presentFamily.has_value();
        }
    };
    //�ṹ�壬���ڴ���swap chain ��ϸ����Ϣ
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
        glfwInit(); // ��ʼ��GLFW��

        glfwWindowHint(GLFW_CLIENT_API,GLFW_NO_API);    //�ر�OpenGL��������
        //glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);     //��ֹ�Զ��������ڴ�С����Ϊ��Ҫ��С�ĵĴ������������
           
        m_window = glfwCreateWindow(WIDTH,HEIGHT,"Vulkan",nullptr,nullptr);    //ʵ�������ڶ���
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
        //������Ⱦѭ��
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
        //�����Ҫ����validationLayer�����Ҽ��ʧ�ܣ��򱨴�
        if (enableValidationLayers && !checkValidationLayerSupport()) {
            throw std::runtime_error("validation layers requested, but not avaliable !!!");
        }

        VkApplicationInfo appInfo{};                    //ʹ��Ĭ��ֵ���VkApplicationInfo�ṹ���еĳ�Ա����
        appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
        appInfo.pApplicationName = "Hello Triangle";
        appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
        appInfo.pEngineName = "No Engine";
        appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
        appInfo.apiVersion = VK_API_VERSION_1_0;

        //������һ��struct��������������Instance���������Ϣ
        VkInstanceCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
        createInfo.pApplicationInfo = &appInfo;
        //����pNextָ��ָ��DebugMessenger��չ��Info
        VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo{};
        
        //��ȡ��Ҫ������extension��Ϣ
        auto extensions = getRequiredExtensions();
        createInfo.enabledExtensionCount = static_cast<uint32_t> (extensions.size());
        createInfo.ppEnabledExtensionNames = extensions.data();
        //����Ƿ�������֤�� - layer
        if (enableValidationLayers) {
            createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
            createInfo.ppEnabledLayerNames = validationLayers.data();
            //����pNextָ��ָ��DebugMessenger��չ��Info
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

        //---------------------- �ⲿ�ֿ��Բ���Ҫ����Ϊ�⽫������֤������� ----------------------
        //��ȡ��ǰSDK����չ֧���б�
        /*uint32_t ExtensionCount = 0;
        vkEnumerateInstanceExtensionProperties(nullptr, &ExtensionCount, nullptr);
        std::vector<VkExtensionProperties> Extensions(ExtensionCount);
        vkEnumerateInstanceExtensionProperties(nullptr, &ExtensionCount, Extensions.data());

        std::cout << "available extensions:\n";

        for (const auto& Extension : Extensions) {
            std::cout << '\t' << Extension.extensionName << '\n';
        }*/

        /* Challenge:����Ƿ�����glfw����չ��Ϣ���ڿ�֧�ֵ���չ�б��� */
        //�������ͷ����� glfwExtension���������͵�const char**, Extensions�е�Ԫ���г�Ա���� char[]�����Խ��бȽ�
        //checkExtensions(Extensions, glfwExtensions, glfwExtensionCount);
        //checkExtensions(Extensions,extensions);

        //����Instance���������ڳ�Ա������
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

    //����surface
    void createSurface() {
        if (glfwCreateWindowSurface(m_instance, m_window, nullptr, &m_surface) != VK_SUCCESS) {
            throw std::runtime_error("failed to create window surface!");
        }
    }
    
    //ѡ�������豸
    void pickPhysicalDevice() {
        //����ͼ�ο�,��ѯ���豸�з���InstanceҪ���ͼ�ο�������
        uint32_t deviceCount = 0;
        vkEnumeratePhysicalDevices(m_instance,&deviceCount,nullptr);
        //������ͼ�ο������� == 0���׳��쳣
        if (deviceCount == 0) {
            throw std::runtime_error("failed to find GPUs with Vulkan support!");
        }
        //�������鱣�����е�ͼ�ο���handle
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

    //�����߼��豸
    void createLogicalDevice() {
        //��ȡqueue famliy������
        QueueFamilyIndices indices = findQueueFamilies(m_physicalDevice);

        std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
        std::set<uint32_t> uniqueQueueFamilies = { indices.graphicsFamily.value(),indices.presentFamily.value() };
        //ָ��queue�����ȼ�
        float queuePriority = 1.0f;

        for (uint32_t queueFamily : uniqueQueueFamilies) {
            //���struct����ָ������queue�Ķ�Ӧ��Ϣ
            VkDeviceQueueCreateInfo queueCreateInfo{};
            queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
            queueCreateInfo.queueFamilyIndex = queueFamily;
            queueCreateInfo.queueCount = 1;
            queueCreateInfo.pQueuePriorities = &queuePriority;
            queueCreateInfos.push_back(queueCreateInfo);
        }

        //ָ���豸֧�ֵ�feature����ǰĬ��ȫ������false��
        VkPhysicalDeviceFeatures deviceFeatures{};

        //�����߼��豸
        //����Info - structure
        VkDeviceCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
        //���queue Info��ָ��
        createInfo.pQueueCreateInfos = queueCreateInfos.data();
        createInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
        //���feature Info��ָ��
        createInfo.pEnabledFeatures = &deviceFeatures;

        /* �������°汾��Vulkan��������device��instance��extension��validation layers������deviceCreateInfo������ֶλᱻ���� */
        //����device��չ
        createInfo.enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size());
        createInfo.ppEnabledExtensionNames = deviceExtensions.data();
        //��device�������֤��
        if (enableValidationLayers) {
            createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
            createInfo.ppEnabledLayerNames = validationLayers.data();
        }
        else {
            createInfo.enabledLayerCount = 0;
        }

        //ʵ�����߼��豸����
        if (vkCreateDevice(m_physicalDevice, &createInfo, nullptr, &m_device) != VK_SUCCESS) {
            throw std::runtime_error("failed to create logical device!");
        }

        vkGetDeviceQueue(m_device, indices.graphicsFamily.value(), 0, &m_graphicsQueue);
        vkGetDeviceQueue(m_device, indices.presentFamily.value(), 0, &m_presentQueue);
    }

    void createSwapChain()
    {
        //���� device �� swapchain ֧�� (details)
        SwapChainSupportDetails swapChainSupport = querySwapChainSupport(m_physicalDevice);
        //�� details ��ѡ������ʵ� swapchain ����
        VkSurfaceFormatKHR surfaceFormat = chooseSwapSurfaceFormat(swapChainSupport.formats);
        VkPresentModeKHR presentMode = chooseSwapPresentMode(swapChainSupport.presentModes);
        VkExtent2D extent = chooseSwapExtent(swapChainSupport.capabilities);
        uint32_t imageCount = swapChainSupport.capabilities.minImageCount + 1;  //���� swapchain �е�ͼ������
        // maxImageCount == 0 ����û�����ֵԼ��
        // ��ʾ�������ֵ������£�����ǰֵ > ���ֵ����imageCount����Ϊ��С��ֵ
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
        createInfo.imageArrayLayers = 1;                                        //ָ��ÿ��ͼ������Ĳ�����
        createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;            //����ͼ�����;��Color Attachment ��ɫ����
                                                                                //��Ҫִ�к�����������Խ���ָ��ΪTRANSFER_DST_BIT,��ͨ���ڴ��������Ⱦͼ���swapchainͼ��֮�佻������

        //��ȡ�����ô���swapchain��imageʹ�õ������豸��queue family��Ϣ
        QueueFamilyIndices indices = findQueueFamilies(m_physicalDevice);
        uint32_t queueFamilyIndices[] = { indices.graphicsFamily.value(), indices.presentFamily.value() };

        //��graphics �� presentation ʹ�ò�ͬ�� queue ����
        //swapchain �У�����ϣ���� graphics ��������Ⱦimage �� Ȼ���� presentation �������ύ image 
        if (indices.graphicsFamily != indices.presentFamily) {
            createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;   //����ģʽ�� �Ӷ��queue���ʵ�ͼ�񣬴���ģʽΪConcurrent��������ʽ������Ȩת��
            createInfo.queueFamilyIndexCount = 2;                       //ʹ�ò���ָ��������Щqueue����image����Ȩ
            createInfo.pQueueFamilyIndices = queueFamilyIndices;
        }
        else {
            createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;    //��ռģʽ�� ��ͬ��queue����ͼ��ʱ��������ʽ���޸�image������Ȩ
            createInfo.queueFamilyIndexCount = 0; // Optional
            createInfo.pQueueFamilyIndices = nullptr; // Optional
        }

        //����pretransform���磺ˮƽ��ת
        createInfo.preTransform = swapChainSupport.capabilities.currentTransform;   //ʹ��currentTransform������Ϊ��pretransform

        //�����Ƿ�ʹ��alphaͨ�����������ڽ��л��
        createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;              //ϣ������alphaͨ��

        //clipped����ΪVK_TRUE���������ı�����window�ڵ���������ʲô��ɫ
        createInfo.presentMode = presentMode;
        createInfo.clipped = VK_TRUE;                                               //���òü�������������

        //���ڴ���swapchainʧЧ��������磺window��С����
        createInfo.oldSwapchain = VK_NULL_HANDLE;                                   //��ʱ���������Ժ�����

        //����swapchain
        if (vkCreateSwapchainKHR(m_device, &createInfo, nullptr, &m_swapChain) != VK_SUCCESS) {
            throw std::runtime_error("failed to create swap chain!");
        }

        //����swapchain�е�ͼ�񣬲������������
        vkGetSwapchainImagesKHR(m_device, m_swapChain, &imageCount, nullptr);
        m_swapChainImages.resize(imageCount);
        vkGetSwapchainImagesKHR(m_device, m_swapChain, &imageCount, m_swapChainImages.data());

        //�����Ա����format �� extent
        m_swapChainImageFormat = surfaceFormat.format;
        m_swapChainExtent = extent;
    }

    //ΪSwapchain�е�ÿ��image����ImageView
    void createImageViews() {
        m_swapChainImageViews.resize(m_swapChainImages.size());

        for (size_t i = 0; i < m_swapChainImages.size(); i++) {
            VkImageViewCreateInfo createInfo{};
            createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;        //����
            createInfo.image = m_swapChainImages[i];                            //����
            createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;                        //1D \ 2D \ 3D Texture or CubeMap
            createInfo.format = m_swapChainImageFormat;                         //32bit RGBA
            
            createInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;            //component���ڻ����ɫͨ��
            createInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
            createInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
            createInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;

            createInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT; //subresourceRange��������ͼ����;���Լ���һ���ֿ��Ա�����
            createInfo.subresourceRange.baseMipLevel = 0;                       //����û���κ�mipmap��layer�� color target
            createInfo.subresourceRange.levelCount = 1;
            createInfo.subresourceRange.baseArrayLayer = 0;
            createInfo.subresourceRange.layerCount = 1;

            //����create API
            if (vkCreateImageView(m_device, &createInfo, nullptr, &m_swapChainImageViews[i]) != VK_SUCCESS) {
                throw std::runtime_error("failed to create image views!");
            }

        }
    }

    //����render pass
    void createRenderPass()
    {
        VkAttachmentDescription colorAttachment{};                                          //attachment˵��������attachment�ĸ�������
        colorAttachment.format = m_swapChainImageFormat;
        colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
        colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;                               //��Ⱦǰ��attachment�е�������δ���
        colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;                             //��Ⱦ��attachment�е�������δ�����Ⱦ�󱣴����ڴ��У�֮����Զ�ȡ���������
        colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;                    //��ʱ������ģ������
        colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;                          //pixel���ڴ��е�layout���Ա��޸ģ�������render pass��ʼ֮ǰ��image layout��ʲô
        colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;                      //ϣ��render pass ������ǽ��ڽ������б�present��image��layout�����⣺��Ϊcolor attachment��layout�������ڴ渴�Ʋ���Ŀ���layout �����ʣ����ߵ��ڴ�layout�кβ�ͬ��
        
        //subpass �� attachment reference
        VkAttachmentReference colorAttachmentRef{};
        colorAttachmentRef.attachment = 0;                                                  //attachment array��s index
        colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        VkSubpassDescription subpass{};
        subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpass.colorAttachmentCount = 1;
        subpass.pColorAttachments = &colorAttachmentRef;                                    //ͨ�� layout(location = 0) out vec4 outColor,������ fragment shader��ֱ���������attachment

        //����subpass dependency����
        VkSubpassDependency dependency{};
        dependency.srcSubpass = VK_SUBPASS_EXTERNAL;                                        //render pass֮ǰ����ʽ��ͨ��ת��
        dependency.dstSubpass = 0;                                                          //subpass������������������ֻ��һ��subpass
        dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;            //ָ��Ҫ�ȴ��Ĳ����Ͳ��������Ľ׶Ρ�����ָ���ȴ�color attachment output�׶�
        dependency.srcAccessMask = 0;
        dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;            //Ҫ�ȴ��Ľ׶���color attachment
        dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;                    //Ҫ�ȴ��Ĳ����� color attachment ��д�����
        /* ��Щ��������ֹת��������ֱ��������д����ɫ֮ǰ */


        //����render pass ����
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
    
    //����render pipeline
    void createGraphicsPipeline()
    {
        //����shader
        std::vector<char> vertShaderCode = readFile("shaders/vert.spv");
        std::vector<char> fragShaderCode = readFile("shaders/frag.spv");
        //����shaderModule
        VkShaderModule vertShaderModule = createShaderModule(vertShaderCode);
        VkShaderModule fragShaderModule = createShaderModule(fragShaderCode);

        /* ... */
        //����vertex shader�� stage info
        VkPipelineShaderStageCreateInfo vertShaderStageInfo{};
        vertShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        vertShaderStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;                             //��pipeline���ĸ��׶�ʹ��shader
        vertShaderStageInfo.module = vertShaderModule;
        vertShaderStageInfo.pName = "main";                                                 //Ҫ���õĺ�����shader����ڵ�
        //����fragment shader �� stage info
        VkPipelineShaderStageCreateInfo fragShaderStageInfo{};
        fragShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        fragShaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
        fragShaderStageInfo.module = fragShaderModule;
        fragShaderStageInfo.pName = "main";

        VkPipelineShaderStageCreateInfo shaderStages[] = { vertShaderStageInfo, fragShaderStageInfo };

        //���� dynamic states �� create info
        /*std::vector<VkDynamicState> dynamicStates = {
            VK_DYNAMIC_STATE_VIEWPORT,
            VK_DYNAMIC_STATE_SCISSOR
        };

        VkPipelineDynamicStateCreateInfo dynamicState{};
        dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
        dynamicState.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
        dynamicState.pDynamicStates = dynamicStates.data();*/

        //���� vertex input stage �� create info
        VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
        vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
        vertexInputInfo.vertexBindingDescriptionCount = 0;
        vertexInputInfo.pVertexBindingDescriptions = nullptr; // Optional
        vertexInputInfo.vertexAttributeDescriptionCount = 0;
        vertexInputInfo.pVertexAttributeDescriptions = nullptr; // Optional

        //���� input assembly stage �� create info
        VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
        inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
        inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        inputAssembly.primitiveRestartEnable = VK_FALSE;

        //���� �ӿ�Viewport �� �ü�scissors
        //Viewport
        VkViewport viewport{};
        viewport.x = 0.0f;
        viewport.y = 0.0f;
        viewport.width = (float)m_swapChainExtent.width;                  //����������ͼ��Ĵ�С������window�ĳߴ粻ͬ
        viewport.height = (float)m_swapChainExtent.height;                //���ڽ������е�ͼ�񽫱�����framebuffer����������ʹviewport�뽻�����е�image��Сһ��
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;
        //scissor
        VkRect2D scissor{};
        scissor.offset = { 0, 0 };
        scissor.extent = m_swapChainExtent;
        //����viewport �� scissor Ϊ��̬state
        std::vector<VkDynamicState> dynamicStates = {
            VK_DYNAMIC_STATE_VIEWPORT,
            VK_DYNAMIC_STATE_SCISSOR
        };

        VkPipelineDynamicStateCreateInfo dynamicState{};
        dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
        dynamicState.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
        dynamicState.pDynamicStates = dynamicStates.data();
        //����viewport �� scissor �� create info
        VkPipelineViewportStateCreateInfo viewportState{};
        viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
        viewportState.viewportCount = 1;
        viewportState.scissorCount = 1;
        //viewportState.pViewports = &viewport;
        //viewportState.pScissors = &scissor;

        //��դ�� state �� create info
        VkPipelineRasterizationStateCreateInfo rasterizer{};
        rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
        rasterizer.depthClampEnable = VK_FALSE;                                                     //�����ã��򳬹���ƽ���Զƽ���fragment����clip������Ⱦshadow mapʱ���������
        rasterizer.rasterizerDiscardEnable = VK_FALSE;                                              //�����ã�����������ϲ���ͨ����դ���׶Σ�������Ͼͽ�ֹ����framebuffer���κ����
        rasterizer.polygonMode = VK_POLYGON_MODE_FILL;                                              //ѡ��ģʽΪ���ģʽ������֮�⻹�� �߿����ģʽ ���� �����ģʽ
        rasterizer.lineWidth = 1.0f;                                                                //�����߿�
        rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;                                                //���޳������ͣ�ǰ���޳� �����޳� �Լ� ȫ���޳�
        rasterizer.frontFace = VK_FRONT_FACE_CLOCKWISE;                                             //ǰ������˳ʱ�붥��˳������ʱ��
        rasterizer.depthBiasEnable = VK_FALSE;                                                      //�Ƿ����������������ͨ����ӳ��� �� ����fragment��б�ʶ�depth����ƫ��
        rasterizer.depthBiasConstantFactor = 0.0f; // Optional
        rasterizer.depthBiasClamp = 0.0f; // Optional
        rasterizer.depthBiasSlopeFactor = 0.0f; // Optional

        //���ز��� state �� create info
        VkPipelineMultisampleStateCreateInfo multisampling{};
        multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
        multisampling.sampleShadingEnable = VK_FALSE;                                               //��ʱ�ر�mutisampling
        multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
        multisampling.minSampleShading = 1.0f; // Optional
        multisampling.pSampleMask = nullptr; // Optional
        multisampling.alphaToCoverageEnable = VK_FALSE; // Optional
        multisampling.alphaToOneEnable = VK_FALSE; // Optional

        //��Ⱥ�ģ����� state �� create info
        //��ʱ��ʹ����Ⱥ�ģ�����

        //��ɫ��� state �� create info
        VkPipelineColorBlendAttachmentState colorBlendAttachment{};                                 //����һ��framebuffer��attachment
        colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
        colorBlendAttachment.blendEnable = VK_TRUE;                                                 //�����ã�����fragment shader �� color �������޸ĵĸ���framebuffer
        colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;                       //��ǰ���ģʽ����Ϊ alpha ���
        colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
        colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
        colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
        colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;

        VkPipelineColorBlendStateCreateInfo colorBlending{};                                        //����������framebuffer��struct���飬����ȫ������
        colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        colorBlending.logicOpEnable = VK_FALSE;                                                     //�Ƿ����ð�λ������㷽ʽ�����Զ����õ�һ�ֻ��ģʽ
        colorBlending.logicOp = VK_LOGIC_OP_COPY; // Optional
        colorBlending.attachmentCount = 1;
        colorBlending.pAttachments = &colorBlendAttachment;
        colorBlending.blendConstants[0] = 0.0f; // Optional
        colorBlending.blendConstants[1] = 0.0f; // Optional
        colorBlending.blendConstants[2] = 0.0f; // Optional
        colorBlending.blendConstants[3] = 0.0f; // Optional

        //���� pipeline layout ���ڴ��� uniform ����
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
        pipelineInfo.subpass = 0;                       // ʹ�� render pass �� subpass index
        //pipeline �̳����
        pipelineInfo.basePipelineHandle = VK_NULL_HANDLE; // Optional
        pipelineInfo.basePipelineIndex = -1; // Optional

        if (vkCreateGraphicsPipelines(m_device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &m_graphicsPipeline) != VK_SUCCESS) {
            throw std::runtime_error("failed to create graphics pipeline!");
        }
        
        //����shaderModule
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
            framebufferInfo.attachmentCount = 1;                                        //Imageview �������������Ҫ����render pass����֮��Ӧ��attachment description
            framebufferInfo.pAttachments = attachments;                                 //Imageview ���������ָ��
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
        poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;               //����command buffer�������¼�¼����������command buffer����һ������
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
        allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;                              //�����command buffer����buffer ���Ǹ���buffer��primary�������ύ��������ִ�У������ܵ�������command buffers��secondary�����ܱ�ֱ���ύ�������Ե�������command buffers
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
        //�����о����пɻ�õ�layer
        uint32_t layerCount;
        vkEnumerateInstanceLayerProperties(&layerCount, nullptr);

        std::vector<VkLayerProperties> avaliableLayers(layerCount);
        vkEnumerateInstanceLayerProperties(&layerCount, avaliableLayers.data());

        //�������validationLayer�еĲ��Ƿ���avaliableLayers�д���
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

        std::vector<const char*> extensions(glfwExtensions, glfwExtensions + glfwExtensionCount); //ʹ��ָ���ַ����ֵ��ָ����һ��ָ������
        if (enableValidationLayers) {
            extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);    //ʹ�ú�����ַ���������ƴд����
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

    //����豸��չ֧��
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

    /* ��DebugMessenger����ʱ�����Info�ṹ���������ȡ����������� */
    void populateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& createInfo) {
        createInfo = {};
        createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
        createInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
        createInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
        createInfo.pfnUserCallback = debugCallback;
    }

    //���ͼ�ο��Ƿ�������ǵ�Ҫ��
    bool isDeviceSuitable(VkPhysicalDevice device) {
        //��ȡ�豸����
        VkPhysicalDeviceProperties deviceProperties;
        vkGetPhysicalDeviceProperties(device, &deviceProperties);
        //��ȡ�豸��ѡ���feature
        VkPhysicalDeviceFeatures deviceFeatures;
        vkGetPhysicalDeviceFeatures(device, &deviceFeatures);
        //���Դ��豸�л�ȡ������Ϣ ........

        //����豸�Ƿ�֧�� ָ�����͵Ķ���family
        QueueFamilyIndices indices = findQueueFamilies(device);

        //����豸�Ƿ�֧�� ָ�����豸��չ
        bool extensionsSupported = checkDeviceExtensionSupport(device);

        //����豸��surface�Ƿ�֧�� swap chain ��ʹ��detail
        bool swapChainAdequate = false;
        if (extensionsSupported) {
            SwapChainSupportDetails swapChainSupport = querySwapChainSupport(device);
            swapChainAdequate = !swapChainSupport.formats.empty() && !swapChainSupport.presentModes.empty();
        }

        //ϣ���豸����Ϊ�����Կ�������֧��geometryShader
        return deviceProperties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU &&
            indices.isComplete() &&
            extensionsSupported &&
            swapChainAdequate;
    }
    
    //�����豸���Ƿ�������Ҫ��Queue families
    QueueFamilyIndices findQueueFamilies(VkPhysicalDevice device) {
        QueueFamilyIndices indices;
        // Assign index to queue families that could be found
        uint32_t queueFamilyCount = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);

        std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
        vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies.data());

        //�ҵ�����һ��֧��VK_QUEUE_GRAPHICS_BIT��family
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

    //���device��swap chain��֧��ϸ��
    SwapChainSupportDetails querySwapChainSupport(VkPhysicalDevice device) {
        SwapChainSupportDetails details;
        //��ѯCapbilities����       physical device �� surface �� swapchain ����ĵ��������
        vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, m_surface, &details.capabilities);
        //��ѯSurface format ����
        uint32_t formatCount;
        vkGetPhysicalDeviceSurfaceFormatsKHR(device, m_surface, &formatCount, nullptr);
        if (formatCount != 0) {
            details.formats.resize(formatCount);
            vkGetPhysicalDeviceSurfaceFormatsKHR(device, m_surface, &formatCount, details.formats.data());
        }
        //��ѯSurface presentation mode����
        uint32_t presentModeCount;
        vkGetPhysicalDeviceSurfacePresentModesKHR(device, m_surface, &presentModeCount, nullptr);
        if (presentModeCount != 0) {
            details.presentModes.resize(presentModeCount);
            vkGetPhysicalDeviceSurfacePresentModesKHR(device, m_surface, &presentModeCount, details.presentModes.data());
        }


        return details;
    }

    //ѡ��Swap chain �� SurfaceFormat                  ����vector�� details�е�formats��Ա����
    VkSurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats) {
        //ÿ��VkSurfaceFormatKHR����������Ա��format �� colorspace
        //format����color��ͨ��������
        //colorspaceʹ��VK_COLOR_SPACE_SRGB_NONLINEAR_KHRָʾ�Ƿ�֧��SRGB��ɫ�ռ�

        //������ʹ��SRGB ��ɫ�ռ� �� 32bit��ɫ��ʽ 
        for (const VkSurfaceFormatKHR& availableFormat : availableFormats) {
            if (availableFormat.format == VK_FORMAT_B8G8R8A8_SRGB && availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
                return availableFormat;
            }
        }

        //������format�����ڣ��򷵻ص�һ��λ�õ�format
        return availableFormats[0];
    }

    //ѡ��Swap chain �� presentation mode
    VkPresentModeKHR chooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes) {
        for (const auto& availablePresentMode : availablePresentModes) {
            if (availablePresentMode == VK_PRESENT_MODE_MAILBOX_KHR) {
                return availablePresentMode;
            }
        }
        return VK_PRESENT_MODE_FIFO_KHR;
    }

    //ѡ��Swap chain �� Swap extent �� �ֱ���  ͨ���봰�ڷֱ���ƥ��
    VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities) {
        //��SurfaceCapbilities�л�ȡ��ǰ���ڵķֱ���
        if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max()) {
            return capabilities.currentExtent;
        }
        else { //����Ӧ���ڷֱ���
            int width, height;
            glfwGetFramebufferSize(m_window,&width,&height);

            VkExtent2D actualExtent = {
                static_cast<uint32_t>(width),
                static_cast<uint32_t>(height)
            };
            //clamp�������ڰ�ʵ�ʵķֱ���Լ����device��surface����ķ�Χ
            actualExtent.width = std::clamp(actualExtent.width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
            actualExtent.height = std::clamp(actualExtent.height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);

            return actualExtent;
        }
    }

    void recordCommandBuffer(VkCommandBuffer commandBuffer, uint32_t imageIndex) {
        VkCommandBufferBeginInfo beginInfo{};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.flags = 0; // Optional                                                                //ָ�����ʹ��command buffer��ִ��һ�κ��������¼�¼������Ϊ��������������ȫλ��renderpass�С����������ύ�����ȴ�ִ��
        beginInfo.pInheritanceInfo = nullptr; // Optional

        if (vkBeginCommandBuffer(commandBuffer, &beginInfo) != VK_SUCCESS) {
            throw std::runtime_error("failed to begin recording command buffer!");
        }

        VkRenderPassBeginInfo renderPassInfo{};
        renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        renderPassInfo.renderPass = m_renderPass;                                                       //��ǰʹ�õ�render pass
        renderPassInfo.framebuffer = m_swapChainFramebuffers[imageIndex];                               //swap chain�еȴ����Ƶ�image��Ӧ��attachment�󶨵�framebuffer
        renderPassInfo.renderArea.offset = { 0, 0 };                                                    //shader�ļ��غͱ����ʲôλ�ÿ�ʼ
        renderPassInfo.renderArea.extent = m_swapChainExtent;                                             //shader�ļ��غͱ���ķ�Χ
        VkClearValue clearColor = { {{0.0f, 0.0f, 0.0f, 1.0f}} };                                       //����attachment�ڼ���ʱ������������ֵ
        renderPassInfo.clearValueCount = 1;
        renderPassInfo.pClearValues = &clearColor;
        //render pass ��ʼ����
        vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);               //��һ����������Ҫ����¼�����command buffer���ڶ���������ָ����render passϸ�ڣ�����������������ṩrender pass�еĻ�ͼָ��

        //��pipline
        vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_graphicsPipeline);          //�ڶ���������graphics pipeline����compute pipeline
        //���ڶ�����viewport��scissors�Ƕ�̬�ģ�������Ҫ��ʽ��������
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


        //�ύ�����εĻ���ָ��
        vkCmdDraw(commandBuffer, 3, 1, 0, 0);                                                           //�ӵڶ���������ʼ�ֱ��ǣ�vertexCount\instanceCount\firstVertex(offset of VertexBuffer)\firstInstance(offset of Instance Draw)

        //��β
        vkCmdEndRenderPass(commandBuffer);
        if (vkEndCommandBuffer(commandBuffer) != VK_SUCCESS) {
            throw std::runtime_error("failed to record command buffer!");
        }
    }

private:
    void drawFrame()
    {
        vkWaitForFences(m_device, 1, &m_inFlightFences[m_currentFrame], VK_TRUE, UINT64_MAX);                            //ʹ��VK_TRUE��Ϊ������ȷ��Fence����������Fence�����źŲż���ִ��.  UINT64_MAX�ǳ�ʱ����

        uint32_t imageIndex;
        //��������������ʱ���ƣ����ĸ��������ź����������������fence�����������������ڽ���image���������ͱ���
        VkResult result = vkAcquireNextImageKHR(m_device, m_swapChain, UINT64_MAX, m_imageAvailableSemaphores[m_currentFrame], VK_NULL_HANDLE, &imageIndex);
        if (result == VK_ERROR_OUT_OF_DATE_KHR) {
            recreateSwapChain();
            return;
        }
        else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
            throw std::runtime_error("failed to acquire swap chain image!");
        }
        vkResetFences(m_device, 1, &m_inFlightFences[m_currentFrame]);                                                   //�ȴ����ֶ���Fence����Ϊ���ź�״̬
        
        //����Command buffer
        vkResetCommandBuffer(m_commandBuffers[m_currentFrame], 0);
        
        //��¼Command buffer
        recordCommandBuffer(m_commandBuffers[m_currentFrame], imageIndex);                                                //����ʵ�ֺõĺ�������command buffer��д������
        
        //�ύCommand buffer
        VkSubmitInfo submitInfo{};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        VkSemaphore waitSemaphores[] = { m_imageAvailableSemaphores[m_currentFrame]};                                   //�ȴ��ĸ��ź�������swap chain�л�ȡimage�Ƿ����
        VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };          //��pipeline���ĸ��׶εȴ���color attachment output�׶�...�ý׶ο�ʼ֮ǰ�ȴ���ȡimage -- ����ζ����image������֮ǰ����ִ��vertex shader
        submitInfo.waitSemaphoreCount = 1;
        submitInfo.pWaitSemaphores = waitSemaphores;
        submitInfo.pWaitDstStageMask = waitStages;                                                      //waitStage�е�ÿ����Ŀ��������waitSemaphores�����е��ź���������һһ��Ӧ
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &m_commandBuffers[m_currentFrame];
        VkSemaphore signalSemaphores[] = { m_renderFinishedSemaphores[m_currentFrame]};                                 //ָ��Command bufferִ����Ϻ󣬻ἤ����Щ�ź���
        submitInfo.signalSemaphoreCount = 1;
        submitInfo.pSignalSemaphores = signalSemaphores;
        if (vkQueueSubmit(m_graphicsQueue, 1, &submitInfo, m_inFlightFences[m_currentFrame]) != VK_SUCCESS) {
            throw std::runtime_error("failed to submit draw command buffer!");
        }

        //presentation����
        VkPresentInfoKHR presentInfo{};
        presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;

        presentInfo.waitSemaphoreCount = 1;                                                             //presentation����Ҫ�ȴ����ź���
        presentInfo.pWaitSemaphores = signalSemaphores;

        VkSwapchainKHR swapChains[] = { m_swapChain };
        presentInfo.swapchainCount = 1;
        presentInfo.pSwapchains = swapChains;                                                           //ָ��swap chain �� swap chain��image������
        presentInfo.pImageIndices = &imageIndex;

        presentInfo.pResults = nullptr; // Optional                                                     //����ָ��VkResult���飬�����ÿ��swap chain�е�image�Ƿ�presentation�ɹ�

        result = vkQueuePresentKHR(m_presentQueue, &presentInfo);                                       //��presentQueue�ύpresent�����ʱ����ͬ��������Ӱ�죬��present��imageһ����render����ˡ�
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
    //ͬ��
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
    catch (const std::exception& e) {           // ��׽ throw �ӳ����쳣�� ��������������Ҫ�� throw �׳�����������ƥ��
        std::cerr << e.what() << std::endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}