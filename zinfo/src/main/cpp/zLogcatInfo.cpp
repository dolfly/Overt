//
// Created by lxz on 2025/8/11.
//

#include <sys/stat.h>
#include "zLog.h"
#include "zLibc.h"
#include "zStdUtil.h"
#include "zShell.h"
#include "zLogcatInfo.h"
#include "zFile.h"

/**
 * 获取日志信息的主函数
 * 检测系统日志中的可疑记录
 * 主要用于检测Zygisk等Root框架的痕迹
 * 注意：这个检测有点不太稳定，一会儿能查到一会儿查不到，但只要查到了肯定是安装了zygiskd
 * @return 包含检测结果的Map，格式：{检测项目 -> {风险等级, 说明}}
 */
map<string, map<string, string>> get_logcat_info(){
    map<string, map<string, string>> info;
    LOGE("get_logcat_info is called");

    // 遍历PID范围，检查每个进程的日志记录
    for(int i = 1500; i < 2000; i++){
        if(i % 10 == 0){
            LOGD("get_logcat_info pid %d", i);
        }

        string pid_str = to_string(i);
        string pid_path_str = "/proc/" + pid_str;
        string cmd_str = "logcat -d | grep avc | grep u:r:su:s0 | grep " + pid_str;

        struct stat st;
        stat(pid_path_str.c_str(), &st);
        string ret = runShell(cmd_str);
        vector<string> ret_split = split_str(ret, '\n');
        
        // 检查日志记录中是否包含可疑的SELinux上下文
        for(string str : ret_split){
            if(strstr(str.c_str(), "u:r:su:s0")){
                LOGI("find zygiskd in logcat %s", str.c_str());
                info["logcat_record"]["risk"] = "error";
                info["logcat_record"]["explain"] = str.c_str();
                return info;
            }
        }
    }
    return info;
}