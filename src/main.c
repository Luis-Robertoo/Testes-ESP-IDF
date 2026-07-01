#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "nvs_flash.h"
#include "esp_log.h"
#include "esp_websocket_client.h"
#include "driver/gpio.h" 
#include "esp_crt_bundle.h"

#define LED_PIN GPIO_NUM_15

// Defina suas credenciais aqui
#define WIFI_SSID      "rede"
#define WIFI_PASS      "senha"
#define WEBSOCKET_URI  "wss://echo.websocket.org" // Servidor de eco público para testes

static const char *TAG = "MEU_APP";

// ============================================================================
// 1. HANDLER DO WEBSOCKET
// Esta função é chamada automaticamente quando o WebSocket conecta, desconecta ou recebe dados
// ============================================================================
static void websocket_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    esp_websocket_event_data_t *data = (esp_websocket_event_data_t *)event_data;
    
    switch (event_id) {
        case WEBSOCKET_EVENT_CONNECTED:
            ESP_LOGI(TAG, "WebSocket Conectado!");
            // Assim que conecta, enviamos uma mensagem de teste
            esp_websocket_client_handle_t client = (esp_websocket_client_handle_t)handler_args;
            char *msg = "Ola Servidor, sou o ESP32-S2!";
            vTaskDelay(5000 / portTICK_PERIOD_MS);
            esp_websocket_client_send_text(client, msg, strlen(msg), portMAX_DELAY);
            break;
            
        case WEBSOCKET_EVENT_DISCONNECTED:
            ESP_LOGW(TAG, "WebSocket Desconectado!");
            break;
            
        case WEBSOCKET_EVENT_DATA:
            ESP_LOGI(TAG, "=== EVENTO DE DADO RECEBIDO ===");
            ESP_LOGI(TAG, "Opcode do frame: 0x%x", data->op_code);
            ESP_LOGI(TAG, "Comprimento dos dados: %d bytes", data->data_len);
            
            if (data->data_len > 0 && data->data_ptr != NULL) {
                printf("Conteudo: %.*s\n", data->data_len, (char *)data->data_ptr);
                fflush(stdout); // <--- FORÇA A SAÍDA NO MONITOR
            } else {
                ESP_LOGW(TAG, "O payload veio vazio ou nulo.");
            }
            break;
    }
}

// ============================================================================
// 2. INICIALIZAÇÃO DO WEBSOCKET
// ============================================================================
static void iniciar_websocket(void)
{
    ESP_LOGI(TAG, "Configurando cliente WebSocket...");

    esp_websocket_client_config_t websocket_cfg = {};
    websocket_cfg.uri = WEBSOCKET_URI;

    websocket_cfg.crt_bundle_attach = esp_crt_bundle_attach;
    //websocket_cfg.skip_cert_common_name_check = true;

    // Cria a instância do cliente
    esp_websocket_client_handle_t client = esp_websocket_client_init(&websocket_cfg);

    // Registra o nosso Handler (a função lá de cima) para escutar os eventos
    esp_websocket_register_events(client, WEBSOCKET_EVENT_ANY, websocket_event_handler, (void *)client);

    // Manda conectar em background
    esp_websocket_client_start(client);
}

// ============================================================================
// 3. HANDLER DO WI-FI
// ============================================================================
static void wifi_event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        printf("Wi-Fi iniciado, tentando conectar...\n");
        fflush(stdout);

        esp_wifi_connect();
        printf("conectado?????\n");
        fflush(stdout);
    } 
    else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
      
        printf("DEBUG: Disconnected, aguardando 2s para reconectar...\n");
        fflush(stdout); // Isso força o log a sair mesmo se o driver USB estiver sobrecarregado
        
        vTaskDelay(2000 / portTICK_PERIOD_MS);
        esp_wifi_connect();
    } 
    else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;

        printf("WIFI_CONNECTED\n");
        printf("IP: " IPSTR "\n", IP2STR(&event->ip_info.ip));
        fflush(stdout);
 
        iniciar_websocket();
    }
}


// ============================================================================
// 4. INICIALIZAÇÃO DO WI-FI
// ============================================================================
static void iniciar_wifi(void)
{
    printf("======1. Inicializa a interface de rede (TCP/IP=======\n");
    fflush(stdout);
    // 1. Inicializa a interface de rede (TCP/IP)
    ESP_ERROR_CHECK(esp_netif_init());
    
    
    printf("======2. Cria o Loop de Eventos padrão do sistema=======\n");
    fflush(stdout);
    // 2. Cria o Loop de Eventos padrão do sistema
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    
    printf("======3. Configura o Wi-Fi com as configurações padrão=======\n");
    fflush(stdout);
    // 3. Configura o Wi-Fi com as configurações padrão
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    
    printf("======4. Registra os Handlers para sabermos quando conectar ou se pegar um IP=======\n");
    fflush(stdout);
    // 4. Registra os Handlers para sabermos quando conectar ou se pegar um IP
    esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL, NULL);
    esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL, NULL);

    
    printf("======5. Passa as credenciais=======\n");
    fflush(stdout);
    // 5. Passa as credenciais
    wifi_config_t wifi_config = {
        .sta = {
            .ssid = WIFI_SSID,
            .password = WIFI_PASS,
        },
    };

    printf("======Set mode e config=======\n");
    fflush(stdout);
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    
    printf("======6. Liga o Wi-Fi (isso vai disparar o evento WIFI_EVENT_STA_START)=======\n");
    fflush(stdout);
    // 6. Liga o Wi-Fi (isso vai disparar o evento WIFI_EVENT_STA_START)
    ESP_ERROR_CHECK(esp_wifi_start());
}

static void tarefa_blink(void *pvParameters)
{
    // Configura o pino como saída
    gpio_reset_pin(LED_PIN);
    gpio_set_direction(LED_PIN, GPIO_MODE_OUTPUT);

    while (1) {
        gpio_set_level(LED_PIN, 1); // Liga o LED
        vTaskDelay(500 / portTICK_PERIOD_MS);
        gpio_set_level(LED_PIN, 0); // Desliga o LED
        vTaskDelay(500 / portTICK_PERIOD_MS);
    }
}
// ============================================================================
// 5. PONTO DE ENTRADA (O equivalente ao Main)
// ============================================================================
void app_main(void)
{
    xTaskCreate(tarefa_blink, "Blink", 2048, NULL, 5, NULL);

    vTaskDelay(8000 / portTICK_PERIOD_MS);

    printf("=========================================\n");
    fflush(stdout);
    printf("Monitor Serial conectado! Iniciando o App...\n");
    fflush(stdout);
    printf("=========================================\n");
    fflush(stdout);

    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
      ESP_ERROR_CHECK(nvs_flash_erase());
      ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    ESP_LOGI(TAG, "Iniciando Aplicativo...");
    
    // Chama a nossa função de configuração do Wi-Fi
    iniciar_wifi();

    while (1) {
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
}