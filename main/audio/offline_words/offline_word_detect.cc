#include "offline_word_detect.h"
#include "application.h"

#include <esp_log.h>
#include <model_path.h>
#include <arpa/inet.h>
#include <sstream>
#include <string.h>

#include "esp_afe_sr_iface.h"
#include "esp_afe_sr_models.h"
#include "esp_mn_iface.h"
#include "esp_mn_models.h"
#include "esp_mn_speech_commands.h"

#include <mutex>

#include <algorithm>
#include <cctype>  // 用于 std::isspace

static const char* TAG = "OfflineWordDetect";
#define OFFLINE_WORD_RUNNING 0x01

OfflineWordDetect::OfflineWordDetect()
    : offline_word_pcm_(),
      offline_word_opus_() {        
}

OfflineWordDetect::~OfflineWordDetect(){

}

void OfflineWordDetect::Initialize(char *model)
{   
    model_ = model;
    if(model_ == nullptr){
        ESP_LOGE(TAG, "OfflineWordDetect model is nullptr");
        exit(1);
    }
    ESP_LOGI(TAG, "OfflineWordDetect model: %s", model_); // 打印命令词模型名称
    AddCommand("bo fang yin yue", [](){
        ESP_LOGI(TAG, "Command: bo fang yin yue");
    }); // 播放音乐
    AddCommand("zan ting", [](){
        ESP_LOGI(TAG, "Command: zan ting");
    }); // 播放音乐
    AddCommand("ni hao le xin", [](){
        ESP_LOGI(TAG, "Command: ni hao le xin");
    }); // 播放音乐

    if(!is_initialized_){
        is_initialized_ = true;
    }
    //vTaskDelay(100 / portTICK_PERIOD_MS); // 玄学的延时
    mutex_.lock();
    // model_data_ = multinet_->create(model_, 3000);  // 设置唤醒后等待事件 6000代表6000毫秒
    // esp_mn_commands_update();    
    // multinet_ = esp_mn_handle_from_name(model_);
    // multinet_->print_active_speech_commands(model_data_);   
    mutex_.unlock();

    // srmodel_list_t *models = esp_srmodel_init("model");
    // ESP_LOGI(TAG, "Model info: %s", models->model_name[0]);
    // model_ = esp_srmodel_filter(models, ESP_MN_PREFIX, ESP_MN_CHINESE);

    // if(model_ == NULL){
    //     ESP_LOGE(TAG, "No multinet model found");
    //     exit(1);
    // }

    // ESP_LOGI(TAG, "multinet_:%s\n", model_); // 打印命令词模型名称    
    // model_data_ = multinet_->create(model_, 6000);  // 设置唤醒后等待事件 6000代表6000毫秒

    // esp_mn_commands_clear(); // 清除当前的命令词列表
    // AddCommand("bo fang yin yue", [](){
    //     ESP_LOGI(TAG, "Command: bo fang yin yue");
    // }); // 播放音乐
    // AddCommand("zan ting", [](){
    //     ESP_LOGI(TAG, "Command: zan ting");
    // }); // 播放音乐
    // AddCommand("ji xu", [](){
    //     ESP_LOGI(TAG, "Command: ji xu");
    // }); // 播放音乐
}


#define OFFLINE_WORD_NUM_MAX 20
esp_err_t OfflineWordDetect::AddCommand(const char* name, std::function<void()> callback)
{
    offline_word_command_t offline_word_command = {.name=name, .callback=callback};//esp_mn的id从1开始
    if(strlen(name) > 255){
        ESP_LOGW(TAG, "The length of offline command name should not exceed 255");
        return ESP_FAIL;
    }
    if(offline_word_commands_.size() >= OFFLINE_WORD_NUM_MAX){
        ESP_LOGW(TAG, "The number of offline commands should not exceed %d", OFFLINE_WORD_NUM_MAX);
        return ESP_FAIL;
    }
    auto it = std::find_if(offline_word_commands_.begin(), offline_word_commands_.end(),
        [name](const offline_word_command_t& cmd){ return cmd.name == name; });
    if(it != offline_word_commands_.end()){
        ESP_LOGW(TAG, "Offline command already exists: %s", name);  
        return ESP_FAIL;
    }
    offline_word_commands_.push_back(offline_word_command);    
    esp_mn_commands_add(2, offline_word_command.name.c_str()); // 添加命令词，不区分id
    esp_mn_commands_update(); // 更新命令词
    ESP_LOGI(TAG, "Add offline command: %s", name);
    return ESP_OK;
}

esp_err_t OfflineWordDetect::DelCommand(const char* name)
{
    int16_t str_len = strlen(name);
    if(str_len > 255){
        ESP_LOGW(TAG, "The length of offline command name should not exceed 255");
        return ESP_FAIL;
    }
    auto it = std::find_if(offline_word_commands_.begin(), offline_word_commands_.end(),
        [name](const offline_word_command_t& cmd){ return cmd.name == name; });
    if(it == offline_word_commands_.end()){
        ESP_LOGW(TAG, "Offline command not found: %s", name);
        return ESP_FAIL;
    }
    esp_mn_commands_remove(it->name.c_str()); // 重新添加命令词，不区分id
    esp_mn_commands_update(); // 更新命令词
    offline_word_commands_.erase(it);    
    ESP_LOGI(TAG, "Delete offline command: %s", name);
    return ESP_OK;
}

static std::string remove_spaces(const std::string& str) {
    std::string result = str;
    result.erase(std::remove_if(result.begin(), result.end(),
                                [](char c) { return c == ' '; }),
                 result.end());
    return result;
}

bool OfflineWordDetect::Detect(int16_t *data)
{
    std::unique_lock<std::mutex> lock(mutex_);
    if(!is_initialized_){
        ESP_LOGE(TAG, "OfflineWordDetect not initialized");
        exit(1);
        return false;
    }
    if(data == NULL){
        ESP_LOGE(TAG, "Input data is NULL");
        return false;
    }
    esp_mn_state_t mn_state = multinet_->detect(model_data_, data); // 检测命令词
    if (mn_state == ESP_MN_STATE_DETECTING) {
        //ESP_LOGI(TAG, "Detecting...");
        return false;
    }

    if (mn_state == ESP_MN_STATE_DETECTED) { // 已检测到命令词
        esp_mn_results_t *mn_result = multinet_->get_results(model_data_); // 获取检测词结果
        for (int i = 0; i < mn_result->num; i++) { // 打印获取到的命令词
            ESP_LOGI(TAG,"TOP %d, command_id: %d, phrase_id: %d, string:%s prob: %f\n", 
            i+1, mn_result->command_id[i], mn_result->phrase_id[i], mn_result->string, mn_result->prob[i]);
            
            auto it = std::find_if(offline_word_commands_.begin(), offline_word_commands_.end(),
            [mn_result](const offline_word_command_t& cmd) {
                return remove_spaces(mn_result->string) == remove_spaces(cmd.name);
            });
            
            if(it != offline_word_commands_.end() && it->callback){
                ESP_LOGI(TAG, "Execute offline command: %s", it->name.c_str());
                it->callback();
                last_detected_offline_word_ = it->name;
                return true;
            }
        }
    }
    return false;
}

bool OfflineWordDetect::HandleResults(esp_mn_results_t *mn_result)
{
    std::unique_lock<std::mutex> lock(mutex_);
    for (int i = 0; i < mn_result->num; i++) { // 打印获取到的命令词
        ESP_LOGI(TAG,"TOP %d, command_id: %d, phrase_id: %d, string:%s prob: %f\n", 
        i+1, mn_result->command_id[i], mn_result->phrase_id[i], mn_result->string, mn_result->prob[i]);
        
        auto it = std::find_if(offline_word_commands_.begin(), offline_word_commands_.end(),
        [mn_result](const offline_word_command_t& cmd) {
            return remove_spaces(mn_result->string) == remove_spaces(cmd.name);
        });
        
        if(it != offline_word_commands_.end() && it->callback){
            ESP_LOGI(TAG, "Execute offline command: %s", it->name.c_str());
            it->callback();
            last_detected_offline_word_ = it->name;
            return true;
        }
        }
    return false;
}

bool OfflineWordDetect::IsInitialized()
{
    return is_initialized_;
}
