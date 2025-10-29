//
// Created by lxz on 2025/7/10.
//

#include <sys/resource.h>
#include "zLog.h"
#include "zLibc.h"
#include "zStd.h"
#include "zFile.h"
#include "zManager.h"
#include "syscall.h"
#include "zSslInfo.h"
#include "zJson.h"
#include "zRootStateInfo.h"
#include "zProcInfo.h"
#include "zSystemPropInfo.h"
#include "zLinkerInfo.h"
#include "zTimeInfo.h"
#include "zPackageInfo.h"
#include "zClassLoaderInfo.h"
#include "zSystemSettingInfo.h"
#include "zPortInfo.h"
#include "zTeeInfo.h"
#include "zSslInfo.h"
#include "zLocalNetworkInfo.h"
#include "zThreadPool.h"
#include "zLogcatInfo.h"
#include "zJavaVm.h"
#include "zSignatureInfo.h"
#include "zSideChannelInfo.h"
#include <mutex>
#include <cmath>


// 静态成员变量初始化
zManager* zManager::instance = nullptr;

// 设备信息存储Map初始化
map<string, map<string, map<string, string>>> zManager::device_info;

/**
 * 获取单例实例
 * 采用线程安全的懒加载模式，首次调用时创建实例
 * @return zManager单例指针
 */
zManager* zManager::getInstance() {
    // 使用 std::call_once 确保线程安全的单例初始化
    static std::once_flag init_flag;
    std::call_once(init_flag, []() {
        try {
            instance = new zManager();
            LOGI("zManager: Created singleton instance");
        } catch (const std::exception& e) {
            LOGE("zManager: Failed to create singleton instance: %s", e.what());
        } catch (...) {
            LOGE("zManager: Failed to create singleton instance with unknown error");
        }
    });
    
    return instance;
}

/**
 * 清理单例实例（主要用于测试或程序退出时）
 */
void zManager::cleanup() {
    if (instance != nullptr) {
        try {
            delete instance;
            instance = nullptr;
            LOGI("zManager: Singleton instance cleaned up");
        } catch (const std::exception& e) {
            LOGE("zManager: Exception during cleanup: %s", e.what());
        } catch (...) {
            LOGE("zManager: Unknown exception during cleanup");
        }
    }
}

/**
 * 构造函数实现
 * 初始化设备信息管理器
 */
zManager::zManager() {
    LOGD("Constructor called");
    // 初始化代码可以在这里添加
}

/**
 * 析构函数实现
 * 清理资源
 */
zManager::~zManager() {
    LOGD("Destructor called");
    // 清理代码可以在这里添加
}

/**
 * 获取所有设备信息
 * 使用读锁保证线程安全，允许多个线程同时读取
 * @return 三层嵌套的Map，包含所有收集到的设备信息
 */
const map<string, map<string, map<string, string>>>& zManager::get_device_info() const{
    LOGD("get_device_info called");
    // 使用共享读锁，允许多个线程同时读取
    std::shared_lock<std::shared_mutex> lock(device_info_mtx_);
    LOGI("get_device_info: device_info size=%zu", device_info.size());
    return device_info;
};

/**
 * 更新设备信息
 * 使用写锁保证线程安全，确保数据一致性
 * @param key 信息类别（如"task_info", "maps_info"等）
 * @param value 该类别下的具体信息
 */
void zManager::update_device_info(const string& key, const map<string, map<string, string>>& value){
    LOGD("update_device_info called, key=%s", key.c_str());
    // 使用独占写锁，确保更新操作的原子性
    std::unique_lock<std::shared_mutex> lock(device_info_mtx_);
    device_info[key] = value;
    LOGI("update_device_info: updated key=%s", key.c_str());
};

const map<string, map<string, string>> zManager::get_info(const string& key){
    LOGD("get_info called, key=%s", key.c_str());
    // 使用独占写锁，确保更新操作的原子性
    std::unique_lock<std::shared_mutex> lock(device_info_mtx_);
    map<string, map<string, string>> info = device_info[key];
    LOGI("update_device_info: updated key=%s", key.c_str());
    return info;
};


void zManager::round_tasks(){
    LOGI("add_tasks: starting periodic task management loop");
    // 定义所有需要周期性执行的任务
    vector<pair<string, void(zManager::*)()>> periodic_tasks = {
            {"root_state_info", &zManager::update_root_state_info},
            {"proc_info", &zManager::update_proc_info},
            {"tee_info", &zManager::update_tee_info},
            {"class_loader_info", &zManager::update_class_loader_info},
            {"linker_info", &zManager::update_linker_info},
            {"package_info", &zManager::update_package_info},
            {"system_setting_info", &zManager::update_system_setting_info},
            {"system_prop_info", &zManager::update_system_prop_info},
            {"signature_info", &zManager::update_signature_info},
            {"port_info", &zManager::update_port_info},
            {"time_info", &zManager::update_time_info},
            {"ssl_info", &zManager::update_ssl_info},
            {"local_network_info", &zManager::update_local_network_info},
            {"logcat_info", &zManager::update_logcat_info},
            {"side_channel_info", &zManager::side_channel_info},
    };
    LOGI("add_tasks: initialized %zu periodic tasks", periodic_tasks.size());
    
    while (true) {
        try {
            // 检查每个任务是否需要执行
            for(const auto& task : periodic_tasks) {
                const string& task_name = task.first;
                if(!zThreadPool::getInstance()->hasTaskName(task_name)){
                     zThreadPool::getInstance()->addTask(task_name, zManager::getInstance(), task.second);
                }
            }
            // 等待一段时间后再次检查
            LOGD("add_tasks: waiting %d seconds before next cycle", time_interval);
            sleep(time_interval*1);
        } catch (const std::exception& e) {
            LOGE("add_tasks: exception occurred: %s", e.what());
            sleep(5); // 发生异常时短暂等待
        } catch (...) {
            LOGE("add_tasks: unknown exception occurred");
            sleep(5); // 发生异常时短暂等待
        }
    }
    LOGI("add_tasks: periodic task management loop stopped");
};

void zManager::update_ssl_info(){
    // 收集SSL信息 - 检测SSL证书异常
    update_device_info("ssl_info", get_ssl_info());
    notice_java("ssl_info");
};

void zManager::update_local_network_info(){
// 收集本地网络信息 - 检测同一网络中的其他Overt设备
    zManager::getInstance()->update_device_info("local_network_info", get_local_network_info());
    notice_java("local_network_info");
};

void zManager::update_proc_info(){
// 收集任务信息 - 检测Frida等调试工具注入的进程
    zManager::getInstance()->update_device_info("proc_info", get_proc_info());
    notice_java("proc_info");
};

void zManager::update_root_state_info(){
// 收集Root文件信息 - 检测Root相关文件
    zManager::getInstance()->update_device_info("root_state_info", get_root_state_info());
    notice_java("root_state_info");
};

void zManager::update_system_prop_info(){
// 收集系统属性信息 - 检测系统配置异常
    zManager::getInstance()->update_device_info("system_prop_info", get_system_prop_info());
    notice_java("system_prop_info");
};

void zManager::update_linker_info(){
// 收集链接器信息 - 检测动态链接库加载异常
    zManager::getInstance()->update_device_info("linker_info", get_linker_info());
    notice_java("linker_info");
};

void zManager::update_port_info(){
    LOGI("update_port_info: starting port info collection");
    zManager::getInstance()->update_device_info("port_info", get_port_info());
    notice_java("port_info");
};

void zManager::update_class_loader_info(){
// 收集类加载器信息 - 检测Java层异常
    zManager::getInstance()->update_device_info("class_loader_info", get_class_loader_info());
    notice_java("class_loader_info");
};

void zManager::update_package_info(){
// 收集包信息 - 检测已安装应用异常
    zManager::getInstance()->update_device_info("package_info", get_package_info());
    notice_java("package_info");
};


void zManager::update_system_setting_info(){
// 收集系统设置信息 - 检测系统设置异常
    zManager::getInstance()->update_device_info("system_setting_info", get_system_setting_info());
    notice_java("system_setting_info");
};


void zManager::update_tee_info(){
// 收集TEE信息 - 检测可信执行环境异常
    update_device_info("tee_info", get_tee_info());
    notice_java("tee_info");
};


void zManager::update_time_info(){
// 收集时间信息 - 检测系统时间异常
    zManager::getInstance()->update_device_info("time_info", get_time_info());
    notice_java("time_info");
};

void zManager::update_logcat_info(){
// 检测系统日志
    zManager::getInstance()->update_device_info("logcat_info", get_logcat_info());
    notice_java("logcat_info");
};

// 侧信道检测
void zManager::side_channel_info(){
    zManager::getInstance()->update_device_info("side_channel_info", get_side_channel_info());
    notice_java("side_channel_info");
};


void zManager::update_signature_info(){
    zManager::getInstance()->update_device_info("signature_info", get_signature_info());
    notice_java("signature_info");
};

void zManager::notice_java(string title){
    LOGI("notice_java: %s", title.c_str());

    // 添加线程安全保护
    static std::mutex notice_java_mutex;
    std::lock_guard<std::mutex> lock(notice_java_mutex);

    LOGI("notice_java: lock by %s ", title.c_str());

    JNIEnv *env = zJavaVm::getInstance()->getEnv();

    if(env == nullptr){
        LOGE("notice_java: env is null");
        return;
    }

    // 查找MainActivity类
    jclass activity_class = zJavaVm::getInstance()->findClass("com/example/overt/MainActivity");
    if (activity_class == nullptr) {
        LOGE("notice_java: activity_class is null");
        return;
    }

    // 获取Java层的回调方法ID
    jmethodID method_id = env->GetStaticMethodID(activity_class, "onCardInfoUpdated", "(Ljava/lang/String;Ljava/lang/String;)V");
    if (method_id == nullptr){
        LOGE("notice_java: method_id onCardInfoUpdated is null");
        return;
    }

    // 使用读锁安全地获取设备信息
    map<string, map<string, string>> card_data = get_info(title);

    zJson card_data_json = card_data;

    string card_data_str = card_data_json.dump();
    LOGE("card_data_str:%s", card_data_str.c_str());

    // 创建Java字符串
    jstring title_jstr = env->NewStringUTF(title.c_str());
    if (title_jstr == nullptr) {
        LOGE("notice_java: Failed to create title string");
        return;
    }

    // 创建Java字符串
    jstring card_data_jstr = env->NewStringUTF(card_data_str.c_str());
    if (card_data_jstr == nullptr) {
        LOGE("notice_java: Failed to create title string");
        return;
    }

    // 调用Java层方法通知信息更新完成
    LOGI("notice_java: calling onCardInfoUpdated for title: %s", title.c_str());
    env->CallStaticVoidMethod(activity_class, method_id, title_jstr, card_data_jstr);

    // 检查是否抛出异常
    if (env->ExceptionCheck()) {
        LOGE("notice_java: Exception occurred during Java method call");
        env->ExceptionDescribe();
        env->ExceptionClear();
    }

    // 清理局部引用
    env->DeleteLocalRef(title_jstr);
    env->DeleteLocalRef(card_data_jstr);

    LOGI("notice_java: completed successfully for title: %s", title.c_str());
}

/**
 * 清空所有设备信息
 * 使用写锁保证线程安全
 * 通常在信息返回给Java层后调用，避免重复返回
 */
void zManager::clear_device_info(){
    LOGD("clear_device_info called");
    // 使用独占写锁，确保清空操作的原子性
    std::unique_lock<std::shared_mutex> lock(device_info_mtx_);
    device_info.clear();
    LOGI("clear_device_info");
};

vector<int> zManager::get_big_core_list() {
    LOGI("get_big_core_list called");
    const int cpuCount = std::thread::hardware_concurrency();

    vector<int> big_cores;
    int max_freq = 0;
    vector<int> freqs;

    LOGD("Scanning CPU frequencies for %d CPUs", cpuCount);
    for (int cpu = 0; cpu < cpuCount; ++cpu) {

        string path = string_format("/sys/devices/system/cpu/cpu%d/cpufreq/cpuinfo_max_freq", cpu);

        LOGV("Checking CPU %d frequency file: %s", cpu, path.c_str());

        zFile cpuinfo_max_freq_file(path);

        string cpu_freq_str = cpuinfo_max_freq_file.readAllText();
        LOGE("freq_str %s", cpu_freq_str.c_str());

        int cpu_freq = atoi(cpu_freq_str.c_str());
        LOGE("cpu_freq %d", cpu_freq);

        freqs[cpu] = cpu_freq;
        if (cpu_freq > max_freq) {
            max_freq = cpu_freq;
            LOGI("New max frequency found: %d kHz (CPU %d)", max_freq, cpu);
        }
    }

    LOGI("Max frequency found: %d kHz", max_freq);
    LOGD("Identifying big cores with max frequency...");

    for (int i = 0; i < cpuCount; ++i) {
        if (freqs[i] == max_freq) {
            big_cores.push_back(i);
            LOGI("Added CPU %d as big core (freq: %d kHz)", i, freqs[i]);
        }
    }

    LOGI("Found %zu big cores out of %d CPUs", big_cores.size(), cpuCount);
    return big_cores;
}


pid_t zManager::gettid(){
    return __syscall0(SYS_gettid);
}

void zManager::bind_self_to_least_used_big_core() {
    LOGI("bind_self_to_least_used_big_core called");
    const int cpuCount = std::thread::hardware_concurrency();

    LOGD("Getting list of big cores...");
    vector<int> big_cores = get_big_core_list();

    if (big_cores.empty()) {
        LOGW("No big cores found, falling back to CPU 0");
        // fallback绑定CPU0
        cpu_set_t cpuset;
        CPU_ZERO(&cpuset);
        CPU_SET(0, &cpuset);
        pid_t tid = gettid();
        LOGD("Attempting to bind thread %d to fallback CPU 0", tid);

        int ret = sched_setaffinity(tid, sizeof(cpuset), &cpuset);
        if (ret == 0) {
            LOGI("Successfully bound thread %d to fallback CPU 0", tid);
        } else {
            LOGE("Failed to bind thread %d to fallback CPU 0 (errno: %d: %s)",
                 tid, errno, strerror(errno));
        }
        return;
    }

    LOGI("Found %zu big cores: [", big_cores.size());
    for (size_t i = 0; i < big_cores.size(); ++i) {
        if (i > 0) LOGI(", ");
        LOGI("%d", big_cores[i]);
    }
    LOGI("]");

    LOGD("Initializing CPU affinity set for big cores...");
    cpu_set_t set;
    // 初始化清空 CPU 集合，否则可能残留旧数据，可能引发未预期的绑定行为。
    CPU_ZERO(&set);

    // 加入目标 CPU 核心 id
    LOGD("Adding big cores to CPU affinity set:");
    for (size_t i = 0; i < big_cores.size(); ++i) {
        CPU_SET(big_cores[i], &set);
        LOGD("  Added CPU %d to affinity set", big_cores[i]);
    }

    // 将一个进程或线程的调度限制在指定的 CPU 核心集合中运行。
    pid_t tid = gettid();
    LOGD("Attempting to bind thread %d to big core set (size: %zu)", tid, big_cores.size());

    int ret = sched_setaffinity(tid, sizeof(set), &set);
    if (ret == 0) {
        LOGI("Successfully bound thread %d to big core set", tid);

        // 验证绑定结果
        cpu_set_t verify_set;
        CPU_ZERO(&verify_set);
        if (sched_getaffinity(tid, sizeof(verify_set), &verify_set) == 0) {
            LOGD("Verification - current CPU affinity:");
            for (int cpu = 0; cpu < cpuCount; ++cpu) {
                if (CPU_ISSET(cpu, &verify_set)) {
                    LOGD("  CPU %d is in affinity set", cpu);
                }
            }
        } else {
            LOGW("Failed to verify CPU affinity (errno: %d: %s)", errno, strerror(errno));
        }
    } else {
        LOGE("Failed to bind thread %d to big core set (errno: %d: %s)",
             tid, errno, strerror(errno));
        if (errno == EINVAL) {
            LOGE("Invalid CPU set or size");
        } else if (errno == EPERM) {
            LOGE("Permission denied - insufficient privileges");
        }
    }
}

void zManager::raise_thread_priority(int nice_priority){
    LOGI("raise_thread_priority called - nice_priority: %d", nice_priority);

    if (nice_priority > 19 || nice_priority < -20) {
        LOGE("Invalid nice value: %d (range: -20 to 19)", nice_priority);
        return;
    }

    pid_t tid = gettid();
    LOGD("Current thread ID: %d", tid);

    int ret = setpriority(PRIO_PROCESS, tid, nice_priority);
    if (ret != 0) {
        LOGE("setpriority failed for thread %d (errno: %d: %s)", tid, errno, strerror(errno));
        if (errno == EPERM) {
            LOGE("Permission denied: You need root or CAP_SYS_NICE to increase priority (negative nice value)");
        }
    } else {
        int actual = getpriority(PRIO_PROCESS, tid);
        LOGI("Successfully set thread %d priority to %d (actual nice: %d)", tid, nice_priority, actual);
    }
}

