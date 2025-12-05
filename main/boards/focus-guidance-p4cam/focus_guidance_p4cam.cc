#include "wifi_board.h"
#include "codecs/es8311_audio_codec.h"
#include "application.h"
#include "display/lcd_display.h"
#include "display/display.h"
#include "button.h"
#include "config.h"

#include "esp_lcd_panel_ops.h"
#include "esp_lcd_mipi_dsi.h"
#include "esp_ldo_regulator.h"

#include "esp_lcd_mipi_dsi.h"
#include "esp_lcd_jd9365_10_1.h"

#include <wifi_station.h>
#include <esp_log.h>
#include <driver/i2c_master.h>
#include <esp_lvgl_port.h>
#include "esp_lcd_touch_gt911.h"

#include "esp32_camera.h"

#define TAG "FocusGuidanceP4Cam"

LV_FONT_DECLARE(font_puhui_20_4);
LV_FONT_DECLARE(font_awesome_20_4);

class FocusGuidanceP4Cam : public WifiBoard {
private:
    i2c_master_bus_handle_t i2c_bus_;
    Button boot_button_;
    LcdDisplay *display__;
    NoDisplay* no_display_;
    Camera* camera_;
    void InitializeCodecI2c() {
        // Initialize I2C peripheral
        i2c_master_bus_config_t i2c_bus_cfg = {
            .i2c_port = I2C_NUM_1,
            .sda_io_num = AUDIO_CODEC_I2C_SDA_PIN,
            .scl_io_num = AUDIO_CODEC_I2C_SCL_PIN,
            .clk_source = I2C_CLK_SRC_DEFAULT,
            .glitch_ignore_cnt = 7,
            .intr_priority = 0,
            .trans_queue_depth = 0,
            .flags = {
                .enable_internal_pullup = 1,
            },
        };
        ESP_ERROR_CHECK(i2c_new_master_bus(&i2c_bus_cfg, &i2c_bus_));
    }

    void InitializeNoLCD() {
        no_display_ = new NoDisplay();
    }
    void InitializeButtons() {
        boot_button_.OnClick([this]() {
            auto& app = Application::GetInstance();
            if (app.GetDeviceState() == kDeviceStateStarting && !WifiStation::GetInstance().IsConnected()) {
                ResetWifiConfiguration();
            }
            app.ToggleChatState(); });
    }

        void InitializeCamera() {
        esp_video_init_sccb_config_t sccb_config = {
            .init_sccb = false,
            .i2c_handle = i2c_bus_,
            .freq = 100000,
        };

        esp_video_init_csi_config_t csi_config = {
            .sccb_config = sccb_config,
            .reset_pin = GPIO_NUM_NC,
            .pwdn_pin = GPIO_NUM_NC,        
        };

        esp_video_init_config_t video_config = {
            .csi = &csi_config,
            //.cam_motor = &cam_motor_config,
        };

        camera_ = new Esp32Camera(video_config);
        camera_->SetHMirror(false);
    }

public:
    FocusGuidanceP4Cam() :
        boot_button_(BOOT_BUTTON_GPIO) {
        InitializeCodecI2c();
        InitializeNoLCD();
        InitializeButtons();
        //InitializeCamera(); 
    }

    virtual AudioCodec *GetAudioCodec() override {
        static Es8311AudioCodec audio_codec(i2c_bus_, I2C_NUM_1, AUDIO_INPUT_SAMPLE_RATE, AUDIO_OUTPUT_SAMPLE_RATE,
                                            AUDIO_I2S_GPIO_MCLK, AUDIO_I2S_GPIO_BCLK, AUDIO_I2S_GPIO_WS, AUDIO_I2S_GPIO_DOUT, AUDIO_I2S_GPIO_DIN,
                                            AUDIO_CODEC_PA_PIN, AUDIO_CODEC_ES8311_ADDR);
        return &audio_codec;
    }

    virtual Display *GetDisplay() override {
        return no_display_;
    }

    virtual Backlight* GetBacklight() override {
        //static PwmBacklight backlight(DISPLAY_BACKLIGHT_PIN, DISPLAY_BACKLIGHT_OUTPUT_INVERT);
        PwmBacklight backlight(GPIO_NUM_26, DISPLAY_BACKLIGHT_OUTPUT_INVERT);
        return &backlight;
    }
};

DECLARE_BOARD(FocusGuidanceP4Cam);
