#ifndef OFFLINE_WORD_DETECT_H
#define OFFLINE_WORD_DETECT_H

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/event_groups.h>

#include <esp_afe_sr_models.h>
#include <esp_nsn_models.h>

#include <list>
#include <string>
#include <vector>
#include <functional>
#include <mutex>
#include <condition_variable>

#include "audio_codec.h"
#include "esp_mn_iface.h"
#include "esp_mn_models.h"
#include "esp_mn_speech_commands.h"

#include "mutex"

typedef struct {
    std::string name;
    std::function<void()> callback;
} offline_word_command_t;

class OfflineWordDetect{
public:
    static OfflineWordDetect& GetInstance() {      
        static OfflineWordDetect instance;
        return instance;
    }
    OfflineWordDetect();
    ~OfflineWordDetect();
 
    //void EncodeOfflineWordData();
    //bool GetOfflineWordOpus(std::vector<uint8_t>& opus);
    const std::string& GetLastDetectedOfflineWord() const { return last_detected_offline_word_; }
    
    esp_err_t AddCommand(const char* name, std::function<void()> callback);
    esp_err_t DelCommand(const char* name);

    //bool IsInitialized();
    bool Detect(int16_t *data);

    bool is_initialized_ = false;
    void Initialize();
private:
    
    char* model_ = NULL;
    std::vector<std::string> offline_words_;
    std::function<void(const std::string& word)> offline_word_detected_callback_;
    AudioCodec* codec_ = nullptr;
    std::string last_detected_offline_word_;

    TaskHandle_t offline_word_encode_task_ = nullptr;
    StaticTask_t offline_word_encode_task_buffer_;
    StackType_t* offline_word_encode_task_stack_ = nullptr;
    std::list<std::vector<int16_t>> offline_word_pcm_;
    std::list<std::vector<uint8_t>> offline_word_opus_;
    std::condition_variable offline_word_cv_;

    std::list<offline_word_command_t> offline_word_commands_;
    esp_mn_iface_t *multinet_;
    model_iface_data_t *model_data_;
    
    // bool is_initialized_ = false;
    std::mutex mutex_;
    
};

#endif // OFFLINE_WORD_DETECT_H
