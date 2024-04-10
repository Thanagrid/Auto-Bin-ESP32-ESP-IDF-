#include <stdio.h>
#include "driver/gpio.h" //GPIO
#include "freertos/FreeRTOS.h" //FreeRTOS for vTaskDelay
#include "freertos/task.h"
#include "esp_log.h"
#include "driver/mcpwm_prelude.h" // PWM

// IR PIN CONFIG
#define IR_PIN GPIO_NUM_2
int val_ir = 0;

// BUZZER PIN CONFIG
#define BUZZER_PIN GPIO_NUM_4

// SERVO CONFIG
static const char *TAG = "MCPWM";
#define SERVO_MIN_PULSEWIDTH_US 500
#define SERVO_MAX_PULSEWIDTH_US 2500
#define SERVO_MIN_DEGREE        -90
#define SERVO_MAX_DEGREE        90

#define SERVO_PULSE_GPIO             13

#define SERVO_TIMEBASE_RESOLUTION_HZ 1000000
#define SERVO_TIMEBASE_PERIOD        20000

// SERVO COMPARE
static inline uint32_t example_angle_to_compare(int angle)
{
    return ((angle - SERVO_MIN_DEGREE) * (SERVO_MAX_PULSEWIDTH_US - SERVO_MIN_PULSEWIDTH_US) / (SERVO_MAX_DEGREE - SERVO_MIN_DEGREE) + SERVO_MIN_PULSEWIDTH_US);
}
int angle = 0;

// IR_IN_PIN CONFIG
#define IR_IN_PIN GPIO_NUM_5
#define GREEN_LED_PIN GPIO_NUM_17
#define RED_LED_PIN GPIO_NUM_16
int val_ir_inBin = 0;

// INPUT PIN
#define GPIO_INPUT_PIN_SET ((1ULL<<IR_PIN) | (1ULL<<IR_IN_PIN))

// OUTPUT PIN
#define GPIO_OUTPUT_PIN_SET ((1ULL<<BUZZER_PIN) | (1ULL<<GREEN_LED_PIN) | (1ULL<<RED_LED_PIN))

void app_main(void){
    // GPIO INPUT CONFIG
    gpio_config_t i_conf = {};
    i_conf.intr_type = GPIO_INTR_DISABLE;
    i_conf.mode = GPIO_MODE_INPUT;
    i_conf.pin_bit_mask = GPIO_INPUT_PIN_SET;
    i_conf.pull_down_en = 0;
    i_conf.pull_up_en = 1;
    gpio_config(&i_conf);

    // GPIO OUTPUT CONFIG
    gpio_config_t o_conf = {};
    o_conf.intr_type = GPIO_INTR_DISABLE;
    o_conf.mode = GPIO_MODE_OUTPUT;
    o_conf.pin_bit_mask = GPIO_OUTPUT_PIN_SET;
    o_conf.pull_down_en = 0;
    o_conf.pull_up_en = 1;
    gpio_config(&o_conf);
    
    // TIMER FOR SERVO
    ESP_LOGI(TAG, "Create timer and operator");
    mcpwm_timer_handle_t timer = NULL;
    mcpwm_timer_config_t timer_config = {
        .group_id = 0,
        .clk_src = MCPWM_TIMER_CLK_SRC_DEFAULT,
        .resolution_hz = SERVO_TIMEBASE_RESOLUTION_HZ,
        .period_ticks = SERVO_TIMEBASE_PERIOD,
        .count_mode = MCPWM_TIMER_COUNT_MODE_UP,
    };
    ESP_ERROR_CHECK(mcpwm_new_timer(&timer_config, &timer));
    mcpwm_oper_handle_t oper = NULL;
    mcpwm_operator_config_t operator_config = {
        .group_id = 0,
    };
    ESP_ERROR_CHECK(mcpwm_new_operator(&operator_config, &oper));
    ESP_LOGI(TAG, "Connect timer and operator");
    ESP_ERROR_CHECK(mcpwm_operator_connect_timer(oper, timer));
    ESP_LOGI(TAG, "Create comparator and generator from the operator");
    mcpwm_cmpr_handle_t comparator = NULL;
    mcpwm_comparator_config_t comparator_config = {
        .flags.update_cmp_on_tez = true,
    };
    ESP_ERROR_CHECK(mcpwm_new_comparator(oper, &comparator_config, &comparator));
    
    mcpwm_gen_handle_t generator = NULL;
    mcpwm_generator_config_t generator_config = {
        .gen_gpio_num = SERVO_PULSE_GPIO,
    };
    ESP_ERROR_CHECK(mcpwm_new_generator(oper, &generator_config, &generator));
    ESP_ERROR_CHECK(mcpwm_comparator_set_compare_value(comparator, example_angle_to_compare(0)));
    ESP_LOGI(TAG, "Set generator action on timer and compare event");

    ESP_ERROR_CHECK(mcpwm_generator_set_action_on_timer_event(generator,
                    MCPWM_GEN_TIMER_EVENT_ACTION(MCPWM_TIMER_DIRECTION_UP, MCPWM_TIMER_EVENT_EMPTY, MCPWM_GEN_ACTION_HIGH)));
    
    ESP_ERROR_CHECK(mcpwm_generator_set_action_on_compare_event(generator,
                    MCPWM_GEN_COMPARE_EVENT_ACTION(MCPWM_TIMER_DIRECTION_UP, comparator, MCPWM_GEN_ACTION_LOW)));
    
    ESP_LOGI(TAG, "Enable and start timer");
    ESP_ERROR_CHECK(mcpwm_timer_enable(timer));
    ESP_ERROR_CHECK(mcpwm_timer_start_stop(timer, MCPWM_TIMER_START_NO_STOP));

    int angle = 0;

    // LOOP APP
    while(1){
        // IR IN 
        val_ir_inBin = gpio_get_level(IR_IN_PIN);
        printf("-- -IN BIN Value : %d ---\n", val_ir_inBin);
        if(val_ir_inBin == 0){
            printf("-> Bin status : RED!!\n");
            gpio_set_level(RED_LED_PIN, 1);
            gpio_set_level(GREEN_LED_PIN, 0);
        }else{
            printf("-> Bin status : GREEN\n");
            gpio_set_level(RED_LED_PIN, 0);
            gpio_set_level(GREEN_LED_PIN, 1);
        }

        // IR DETECTION
        val_ir = gpio_get_level(IR_PIN);
        printf("--- IR Value : %d ---\n", val_ir);
        if(val_ir == 0){
            printf("-> Bin open!!\n");
            angle = 90;
            ESP_ERROR_CHECK(mcpwm_comparator_set_compare_value(comparator, example_angle_to_compare(angle)));
            gpio_set_level(BUZZER_PIN, 1);
            vTaskDelay(100 / portTICK_PERIOD_MS);
            gpio_set_level(BUZZER_PIN, 0);
            vTaskDelay(3000 / portTICK_PERIOD_MS);
        }else{
            printf("-> Bin close\n");
            angle = -90;
            ESP_ERROR_CHECK(mcpwm_comparator_set_compare_value(comparator, example_angle_to_compare(angle)));
        }
        printf("\n");

            vTaskDelay(100 / portTICK_PERIOD_MS);
        }

}