#pragma once

#include "esphome/core/component.h"
#include "esphome/components/uart/uart.h"

namespace esphome {
namespace TuyaCoverCustom {
public:
    // TuyaCoverCustom(int control_datapoint);
    // TuyaCoverCustom(int position_datapoint);
    // TuyaCoverCustom(int position_report_datapoint);
    // TuyaCoverCustom(int direction_datapoint);
    // TuyaCoverCustom(int min_value);
    // TuyaCoverCustom(int max_value);
    // TuyaCoverCustom(bool invert_position);
    // void register_service(TeleInfoListener *listener);
    // register_service(&CustomAPI::setStatusReport, "get_status_report");
    // register_service(&CustomAPI::setMotorNormal, "set_motor_normal");
    // register_service(&CustomAPI::setMotorReversed, "set_motor_reversed");
    // register_service(&CustomAPI::sendCommand, "send_command", {"data"});
    void setup() override
    {
        register_service(&CustomAPI::setStatusReport, "get_status_report");
        register_service(&CustomAPI::setMotorNormal, "set_motor_normal");
        register_service(&CustomAPI::setMotorReversed, "set_motor_reversed");
        register_service(&CustomAPI::sendCommand, "send_command", {"data"});
    }

    void setStatusReport()
    {
        ESP_LOGI(TAG, "ZM79EDT_COMMAND = QUERY_STATUS");
        write_command(ZM79EDT_QUERY_STATUS, 0, 0);
    }

    void setMotorNormal()
    {
        ESP_LOGI(TAG, "ZM79EDT_COMMAND = ENABLE_REVERSING");
        write_command(ZM79EDT_COMMAND, zm79edt_enable_reversing, sizeof(zm79edt_enable_reversing));
    }

    void setMotorReversed()
    {
        ESP_LOGI(TAG, "ZM79EDT_COMMAND = ENABLE_REVERSING");
        write_command(ZM79EDT_COMMAND, zm79edt_disable_reversing, sizeof(zm79edt_disable_reversing));
    }

    void sendCommand(std::string data)
    {
        int i = 0;
        uint8_t sum = 0;
        while (i < data.length())
        {
            const char hex[2] = {data[i++], data[i++]};
            uint8_t d = strtoul(hex, NULL, 16);
            sum += d;
            writeByte(d);
        }
        writeByte(sum);
    }

    void writeByte(uint8_t data)
    {
        Serial.write(data);
    }
};

class TuyaCoverCustomCurtain : public Component, public Cover {
protected:

public:
    void setup() override
    {
        ESP_LOGI(TAG, "ZM79EDT_COMMAND = QUERY_STATUS");
        write_command(ZM79EDT_QUERY_STATUS, 0, 0);
    }

    CoverTraits get_traits() override
    {
        auto traits = CoverTraits();
        traits.set_is_assumed_state(false);
        traits.set_supports_position(true);
        traits.set_supports_tilt(false);
        return traits;
    }

    void control(const CoverCall &call) override
    {
        if (call.get_position().has_value())
        {
            // Write pos (range 0-1) to cover
            // Cover pos (range 0x00-0x64) (closed - open)
            uint8_t pos = (*call.get_position() * 100);
            ESP_LOGV(TAG, "POS = %d", pos);

            switch (pos)
            {
            case 0:
                ESP_LOGI(TAG, "ZM79EDT_COMMAND = CLOSE");
                write_command(ZM79EDT_COMMAND, zm79edt_close, sizeof(zm79edt_close));
                break;
            case 100:
                ESP_LOGI(TAG, "ZM79EDT_COMMAND = OPEN");
                write_command(ZM79EDT_COMMAND, zm79edt_open, sizeof(zm79edt_open));
                break;
            default:
                zm79edt_pos[7] = pos;
                ESP_LOGI(TAG, "ZM79EDT_COMMAND = POS = %d%%", pos);
                write_command(ZM79EDT_COMMAND, zm79edt_pos, sizeof(zm79edt_pos));
                break;
            }
            // publish_state only when position is confirmed in loop()
        }
        if (call.get_stop())
        {
            // User requested cover stop
            ESP_LOGI(TAG, "ZM79EDT_COMMAND = STOP");
            write_command(ZM79EDT_COMMAND, zm79edt_stop, sizeof(zm79edt_stop));
        }
    }

    void loop() override
    {
        unsigned long currentHeartbeatMillis = millis();
        if (currentHeartbeatMillis - previousHeartbeatMillis >= HEARTBEAT_INTERVAL_MS)
        {
            previousHeartbeatMillis += HEARTBEAT_INTERVAL_MS;
            ESP_LOGI(TAG, "ZM79EDT_COMMAND = HEARTBEAT");
            write_command(ZM79EDT_HEARTBEAT, 0, 0);
        }

        bool have_message = read_command();

        if(have_message && command_.command == ZM79EDT_HEARTBEAT)
        {
            ESP_LOGI(TAG, "ZM79EDT_RESPONSE = %s", (command_.value[0] == 0) ? "FIRST_HEARTBEAT" : "HEARTBEAT");  
        }
        else if (have_message && command_.command == ZM79EDT_RESPONSE)
        {
            switch (command_.value[0])
            {
            case ZM79EDT_DPID_POSITION:
                ESP_LOGI(TAG, "ZM79EDT_DPID_POSITION = %d%%", command_.value[7]);
                this->position = ((command_.value[7]) / 100.0f);
                this->publish_state();
                break;
            case ZM79EDT_DPID_DIRECTION:
                ESP_LOGI(TAG, "ZM79EDT_DPID_DIRECTION = 0x%02X", command_.value[4]);
                break;
            case ZM79EDT_DPID_UNKNOWN:
                ESP_LOGI(TAG, "ZM79EDT_DPID_UNKNOWN ENUM = 0x%02X", command_.value[4]);
                break;
            case ZM79EDT_DPID_ERROR:
                ESP_LOGI(TAG, "ZM79EDT_DPID_ERROR BITMAP = 0x%02X", command_.value[4]);
                break;
            default:
                break;
            }
        }
    };
}  // namespace tuyacovercustom
}  // namespace esphome
