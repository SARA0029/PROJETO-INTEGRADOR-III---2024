/*
 * SPDX-FileCopyrightText: 2021-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_err.h"
#include "esp_log.h"
#include "driver/i2c_master.h"
#include "esp_lvgl_port.h"
#include "driver/gpio.h"
#include "lvgl.h"
#include "i2c_oled.h"
#include "cbspI2C.h"

#include "cBMP280.h"
#include "cSMP3011.h"
#include "button.h"
static const char *TAG = "example";


/*
        HARDWARE DEFINITIONS
*/
#define I2C_BUS_PORT                  0
#define EXAMPLE_PIN_NUM_SDA           GPIO_NUM_5
#define EXAMPLE_PIN_NUM_SCL           GPIO_NUM_4
#define EXAMPLE_PIN_LED               GPIO_NUM_16               
#define EXAMPLE_PIN_BUTTON           GPIO_NUM_25  

/*
        Components
*/
cbspI2C I2CChannel1;
cBMP280 BMP280;
cSMP3011 SMP3011;

/*
        TASKS
*/
//Prototypes
void TaskBlink(void *parameter);
void TaskDisplay(void *parameter);
void TaskSensors(void *parameter);

//Handlers
TaskHandle_t taskBlinkHandle   = nullptr;
TaskHandle_t taskDisplayHandle = nullptr;
TaskHandle_t taskSensorsHandle = nullptr;



Button button(EXAMPLE_PIN_BUTTON);
//Main function
extern "C"
void app_main()
{

    //Setup pin for LED
     // Configurar pino de LED
    gpio_set_direction(EXAMPLE_PIN_LED, GPIO_MODE_OUTPUT);

    button.init(); 
    //Setup I2C 0 for display
    ESP_LOGI(TAG, "Initialize I2C bus");
    i2c_master_bus_handle_t i2c_bus = NULL;
    i2c_master_bus_config_t bus_config = {
        .i2c_port = I2C_BUS_PORT,
        .sda_io_num = EXAMPLE_PIN_NUM_SDA,
        .scl_io_num = EXAMPLE_PIN_NUM_SCL,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .intr_priority = 0,
        .trans_queue_depth = 0,
        .flags
        {
            .enable_internal_pullup = true,
        }
    
    };
    ESP_ERROR_CHECK(i2c_new_master_bus(&bus_config, &i2c_bus));

    
    //Setup I2C 1 for sensors
    I2CChannel1.init(I2C_NUM_1, GPIO_NUM_33, GPIO_NUM_32);
    I2CChannel1.openAsMaster(100000);

    //Initialize sensors
    BMP280.init(I2CChannel1);
    SMP3011.init(I2CChannel1);

    //Initialize display
    i2c_oled_start(i2c_bus);
    
    //Create tasks
    xTaskCreate( TaskBlink  , "Blink"  , 1024,  nullptr,   tskIDLE_PRIORITY,  &taskBlinkHandle   );  
    xTaskCreate( TaskSensors, "Sensors", 2048,  nullptr,   tskIDLE_PRIORITY,  &taskSensorsHandle );    
    xTaskCreate( TaskDisplay, "Display", 4096,  nullptr,   tskIDLE_PRIORITY,  &taskDisplayHandle );
   
}

static int conta = 0;  // 0 = PSI, 1 = Bar
void TaskDisplay(void *parameter)
{
    lvgl_port_lock(0);
    lv_obj_t *scr = lv_disp_get_scr_act(nullptr);

    lv_obj_t *labelPressure = lv_label_create(scr);
    lv_label_set_long_mode(labelPressure, LV_LABEL_LONG_SCROLL_CIRCULAR);
    lv_label_set_text(labelPressure, "");
    lv_obj_set_width(labelPressure, 128);
    lv_obj_align(labelPressure, LV_ALIGN_TOP_MID, 25, 20);
    lv_obj_set_style_text_font(labelPressure, &lv_font_montserrat_14, 0);

    lv_obj_t *labelTemperature = lv_label_create(scr);
    lv_label_set_long_mode(labelTemperature, LV_LABEL_LONG_SCROLL_CIRCULAR);
    lv_label_set_text(labelTemperature, "");
    lv_obj_set_width(labelTemperature, 128);
    lv_obj_align(labelTemperature, LV_ALIGN_TOP_MID, -8, 50);

    lvgl_port_unlock();

  
    float pressureReadings[100];  // Aumentamos o número de leituras possíveis
    int leituras = 0;             // Índice para as leituras
    bool showAverage = false;     // Flag para exibir a média
    bool ledOff = false;          // Flag para o estado do LED
    float pressureAverage = 0.0f;
    TickType_t timeStart = 0;     // Marca o início do tempo para exibir a média

    while (1)
    {
        // Verifica se o botão foi pressionado
        if (gpio_get_level(EXAMPLE_PIN_BUTTON) == 0)  // Botão pressionado (pino com pull-up)
        {
            // Alterna entre PSI (0) e Bar (1)
            conta = (conta == 0) ? 1 : 0;
            vTaskDelay(200 / portTICK_PERIOD_MS);  // Delay para debounce (evitar múltiplos acionamentos)
        }

        // Coleta leituras por 5 segundos
        if (xTaskGetTickCount() - timeStart < pdMS_TO_TICKS(5000))
        {
            // Obtém a pressão do sensor
            float pressure = SMP3011.getPressure();

            if (pressure <= 0) {
                pressure = 0.0f;  // Se a pressão for menor ou igual a zero, define como zero
            }
                
            if (pressure > 0)
            {
                // Armazena a leitura de pressão
                pressureReadings[leituras++] = pressure;
            }
        }
        else
        {
            // Após 5 segundos, selecione as 10 maiores leituras
            if (leituras > 0)
            {
                // Ordena as leituras em ordem decrescente
                for (int i = 0; i < leituras - 1; i++)
                {
                    for (int j = i + 1; j < leituras; j++)
                    {
                        if (pressureReadings[i] < pressureReadings[j])
                        {
                            // Troca as leituras
                            float temp = pressureReadings[i];
                            pressureReadings[i] = pressureReadings[j];
                            pressureReadings[j] = temp;
                        }
                    }
                }

                // Pega as 10 maiores leituras
                float sum = 0.0f;
                for (int i = 0; i < 10 && i < leituras; i++)
                {
                    sum += pressureReadings[i];
                }
                pressureAverage = sum / 10.0f;

                // Definir o tempo para mostrar a média por 5 segundos
                showAverage = true;
                timeStart = xTaskGetTickCount(); // Reinicia o contador para o tempo de exibição da média

                // Apaga o LED
                gpio_set_level(EXAMPLE_PIN_LED, 0);

                // Resetando leituras para o próximo ciclo de 5 segundos
                leituras = 0;
            }
        }

        // Atualiza o valor de pressão conforme a variável conta
        lvgl_port_lock(0);

        if (showAverage)
        {
            // Exibe a média das leituras por 5 segundos
            if (conta == 0)
            {
                // Exibe a média em PSI
                lv_label_set_text_fmt(labelPressure, "%.2f PSI", pressureAverage * 0.00014504);
            }
            else
            {
                // Exibe a média em Bar
                lv_label_set_text_fmt(labelPressure, "%.2f Bar", pressureAverage * 0.000986923);
            }

            lv_obj_align(labelPressure, LV_ALIGN_TOP_MID, 25, 20);

            // Verifica se o tempo de 2 segundos passou
            if ((xTaskGetTickCount() - timeStart) * portTICK_PERIOD_MS >= 2000)
            {
                showAverage = false;  // Para de exibir a média
                ledOff = true;        // Sinaliza que o LED pode ser ligado novamente
            }
        }
        else
        {
            // Exibe a pressão em PSI ou Bar dependendo da variável conta
            if (conta == 0)
            {
                // Exibe a pressão em PSI
                lv_label_set_text_fmt(labelPressure, "%6.2f PSI", SMP3011.getPressure() * 0.00014504);
            }
            else
            {
                // Exibe a pressão em Bar
                lv_label_set_text_fmt(labelPressure, "%6.2f Bar", SMP3011.getPressure() * 0.000986923);
            }
        }

        // Atualiza a temperatura
        float temperature = SMP3011.getTemperature();  // Obtém a temperatura
    
        lv_label_set_text_fmt(labelTemperature, "%6.2f C", temperature);

        lvgl_port_unlock();

        // Piscando o display e o LED
        if (showAverage && (xTaskGetTickCount() / pdMS_TO_TICKS(500)) % 2 == 0)
        {
            // Piscando a média a cada 500 ms
            lv_label_set_text(labelPressure, "");
        }

        // Liga o LED novamente após a exibição
        if (ledOff)
        {
            gpio_set_level(EXAMPLE_PIN_LED, 1); // Liga o LED novamente
            ledOff = false;
        }

        vTaskDelay(100 / portTICK_PERIOD_MS);
    }
}



void TaskBlink(void *parameter)
{
    while(1)
    {
        gpio_set_level(EXAMPLE_PIN_LED, 1); 
        vTaskDelay(500/portTICK_PERIOD_MS);
        
    }
}

void TaskSensors(void *parameter)
{
    while(1)
    {
        BMP280.poll();
        SMP3011.poll();
        vTaskDelay(10/portTICK_PERIOD_MS);
    }
}