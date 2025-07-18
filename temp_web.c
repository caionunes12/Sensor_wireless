//============================================================
// Projeto: Monitor de Temperatura com Wi-Fi
// Descrição: Monitora a temperatura ambiente e controla um Cooler Fan via PWM
// Autores: Caio, Felipe, Osvaldo e Domingos
// Instituição: UNIVASF - Universidade Federal do Vale do São Francisco
// Data: 16/07/2025
//============================================================

//Bibliotecas necessárias
//============================================================
#include <stdio.h>      // entrada e saída padrão
#include <string.h>     // manipulação de strings
#include <stdlib.h>     // manipulação de memória
#include <math.h>       // funções matemáticas

#include "pico/stdlib.h"        // funções de I/O
#include "hardware/adc.h"       // controle de ADC
#include "hardware/pwm.h"       // controle de PWM
#include "pico/cyw43_arch.h"    // Wi-Fi e GPIO
#include "lwip/pbuf.h"      // estruturas de buffer
#include "lwip/tcp.h"       // estruturas de controle de protocolo
#include "lwip/netif.h"     // estruturas de interface de rede


//. ============================================================
// Definições de hardware e parâmetros
//. ============================================================
#define WIFI_SSID "SUA_REDE" // Nome da rede Wi-Fi
#define WIFI_PASSWORD "SUA_SENHA"   // Senha da rede Wi-Fi

#define VRX_PIN 28  // Pino do ADC para leitura de temperatura
#define LED_PIN 16 // Pino do LED PWM
#define WIFI_LED_PIN CYW43_WL_GPIO_LED_PIN // Pino do LED indicador de Wi-Fi

#define BETA     3435.0     // Constante beta do NTC
#define R0       10000.0    // Resistência do NTC a 25°C
#define T0       298.15     // Temperatura de referência em Kelvin (25°C)
#define R_FIXED  10000.0    // Resistência fixa para o divisor de tensão
#define VCC      3.3        // Tensão de alimentação do ADC (3.3V)

#define PWM_WRAP 4095       // Valor máximo do contador PWM (12 bits)

//============================================================
// Variáveis globais

static float current_temperature = 0.0;     // Temperatura atual em °C
static float current_pwm_percent = 0.0;     // Percentual de duty cycle do PWM
static bool alarm_active = false;           // Indica se o alarme de temperatura está ativo

//============================================================
// Prototipação de funções

uint pwm_init_gpio(uint gpio, uint wrap);           // Inicializa o GPIO para PWM          
void set_pwm_percent(uint gpio, float percent);     // Define o percentual de duty cycle do PWM
void update_temperature_system(void);               // Atualiza a temperatura e o PWM com base na leitura do ADC
static err_t tcp_server_accept(void *arg, struct tcp_pcb *newpcb, err_t err);       // Aceita conexões TCP
static err_t tcp_server_recv(void *arg, struct tcp_pcb *tpcb, struct pbuf *p, err_t err);       // Recebe dados do cliente TCP
void user_request(char **request);      // Processa requisições do usuário via HTTP

//============================================================
// Função principal

int main() {
    stdio_init_all();                       // Inicializa a entrada e saída padrão

    adc_init();                             // Inicializa o ADC
    adc_gpio_init(VRX_PIN);                 // Inicializa o pino do ADC

    pwm_init_gpio(LED_PIN, PWM_WRAP);       // Inicializa o pino do LED para PWM

    while (cyw43_arch_init()) {             // Inicializa o Wi-Fi
        printf("Falha ao inicializar Wi-Fi\n");
        sleep_ms(100);
        return -1;
    }

    cyw43_arch_gpio_put(WIFI_LED_PIN, 0);
    cyw43_arch_enable_sta_mode();

    printf("Conectando ao Wi-Fi...\n");         // Tenta conectar ao Wi-Fi
    while (cyw43_arch_wifi_connect_timeout_ms(WIFI_SSID, WIFI_PASSWORD, CYW43_AUTH_WPA2_AES_PSK, 20000)) {
        printf("Falha ao conectar ao Wi-Fi\n");
        sleep_ms(100);
        return -1;
    }
    printf("Conectado ao Wi-Fi\n");

    sleep_ms(2000);
    if (netif_default) {        // Verifica se a interface de rede está disponível
        printf("\n=== INFORMACOES DE REDE ===\n");
        printf("IP do dispositivo: %s\n", ipaddr_ntoa(&netif_default->ip_addr));
        printf("Acesse: http://%s\n", ipaddr_ntoa(&netif_default->ip_addr));
        printf("Porta: 80\n");
        printf("===========================\n\n");
    } else {
        printf("ERRO: Interface de rede nao disponivel!\n");
        return -1;
    }

    struct tcp_pcb *server = tcp_new();     // Cria um novo PCB TCP
    if (!server) {
        printf("Falha ao criar servidor TCP\n");
        return -1;
    }

    if (tcp_bind(server, IP_ADDR_ANY, 80) != ERR_OK) {      // Associa o servidor à porta 80
        printf("Falha ao associar servidor TCP à porta 80\n");
        return -1;
    }

    server = tcp_listen(server);        // Coloca o servidor em modo de escuta
    tcp_accept(server, tcp_server_accept);
    printf("Servidor ouvindo na porta 80\n");

    uint32_t last_print = 0;
    printf("Iniciando monitoramento de temperatura...\n");
    sleep_ms(1000);

    while (true) {      // Loop principal do programa
        update_temperature_system();        // Atualiza a temperatura e o PWM

        uint32_t now = to_ms_since_boot(get_absolute_time());       // Obtém o tempo atual em milissegundos
        if (now - last_print >= 1000) {
            printf("Temp: %.2f °C | PWM: %.1f%%\n", current_temperature, current_pwm_percent);
            if (alarm_active) {
                printf("⚠️  ALARME! Temperatura crítica!\n");
            }
            last_print = now;
        }

        cyw43_arch_poll();      // Polling do Wi-Fi para manter a conexão ativa
        sleep_ms(100);
    }

    cyw43_arch_deinit();        // Desativa o Wi-Fi antes de sair
    return 0;
}

//============================================================
// Implementação das funções

uint pwm_init_gpio(uint gpio, uint wrap) {           // Inicializa o GPIO para PWM
    gpio_set_function(gpio, GPIO_FUNC_PWM);          // Configura o pino como função PWM
    uint slice_num = pwm_gpio_to_slice_num(gpio);    // Obtém o número do slice PWM associado ao GPIO
    pwm_set_wrap(slice_num, wrap);                  // Define o valor máximo do contador PWM
    pwm_set_enabled(slice_num, true);               // Habilita o PWM no slice
    return slice_num;
}

void set_pwm_percent(uint gpio, float percent) {        // Define o percentual de duty cycle do PWM
    if (percent < 0) percent = 0;
    if (percent > 100) percent = 100;
    uint16_t level = (uint16_t)(percent / 100.0 * PWM_WRAP);
    pwm_set_gpio_level(gpio, level);
}

void update_temperature_system(void) {      // Atualiza a temperatura e o PWM com base na leitura do ADC
    adc_select_input(0);
    uint16_t adc_raw = adc_read();
    double v_out = (adc_raw / 4095.0) * VCC;
    if (v_out <= 0.0) return;

    double r_ntc = R_FIXED * (VCC / v_out - 1.0);       // Calcula a resistência do NTC
    double temp_k = 1.0 / (1.0 / T0 + (1.0 / BETA) * log(r_ntc / R0));      // Calcula a temperatura em Kelvin
    double temp_c = temp_k - 273.15;        // Converte para Celsius
    current_temperature = temp_c;       // Atualiza a temperatura atual

    float duty_percent = 0;         // Inicializa o percentual de duty cycle
    if (temp_c >= 40.0 && temp_c <= 60.0) {
        duty_percent = (temp_c - 40.0) / 20.0 * 100.0;
        alarm_active = false;
    } else if (temp_c > 60.0) {
        duty_percent = 100.0;
        alarm_active = true;
    } else {
        duty_percent = 0;
        alarm_active = false;
    }

    current_pwm_percent = duty_percent;     // Atualiza o percentual de duty cycle atual
    set_pwm_percent(LED_PIN, duty_percent);
}

static err_t tcp_server_accept(void *arg, struct tcp_pcb *newpcb, err_t err) {      // Aceita conexões TCP
    tcp_recv(newpcb, tcp_server_recv);
    return ERR_OK;
}

void user_request(char **request) {     // Processa requisições do usuário via HTTP
    if (strstr(*request, "GET /wifi_on") != NULL) {
        cyw43_arch_gpio_put(WIFI_LED_PIN, 1);
    } else if (strstr(*request, "GET /wifi_off") != NULL) {
        cyw43_arch_gpio_put(WIFI_LED_PIN, 0);
    }
}

static err_t tcp_server_recv(void *arg, struct tcp_pcb *tpcb, struct pbuf *p, err_t err) {      // Recebe dados do cliente TCP
    if (!p) {
        tcp_close(tpcb);
        tcp_recv(tpcb, NULL);
        return ERR_OK;
    }

    char *request = (char *)malloc(p->len + 1);     // Aloca memória para a requisição
    memcpy(request, p->payload, p->len);
    request[p->len] = '\0';
    printf("Request: %s\n", request);
    user_request(&request);

    char html[1024];        // Buffer para a resposta HTML
    const char *bg_color, *temp_color;
    if (current_temperature < 20.0) {       // Define cores com base na temperatura
        bg_color = "#e6f3ff";
        temp_color = "#0066cc";
    } else if (current_temperature < 40.0) {
        bg_color = "#e6ffe6";
        temp_color = "#00cc00";
    } else if (current_temperature < 60.0) {
        bg_color = "#fff2e6";
        temp_color = "#ff9900";
    } else {
        bg_color = "#ffe6e6";
        temp_color = "#cc0000";
    }

    // == Gera a resposta HTML ==
    // Formata a resposta HTML com as informações do sistema

    snprintf(html, sizeof(html),       
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: text/html; charset=utf-8\r\n"
        "Refresh: 3\r\n"
        "\r\n"
        "<!DOCTYPE html><html><head><title>Monitor de Temperatura</title><meta charset=\"utf-8\">"
        "<style>body{font-family:Arial;text-align:center;background:%s;margin:20px;}"
        "h1{color:#333;}.temp{font-size:48px;color:%s;margin:30px;}"
        ".alarm{color:red;font-size:24px;margin:20px;}.status{font-size:20px;margin:20px;}"
        "button{font-size:18px;padding:10px 20px;margin:10px;background:#4CAF50;color:white;border:none;border-radius:5px;}"
        "button:hover{background:#45a049;}</style></head><body>"
        "<h1>Monitor de Temperatura</h1><div class=\"temp\">%.1f°C</div>%s"
        "<div class=\"status\"><b>Status da Ventoinha:</b> %s<br><b>PWM:</b> %.1f%%</div>"
        "<form action=\"./wifi_on\"><button>WiFi LED ON</button></form>"
        "<form action=\"./wifi_off\"><button>WiFi LED OFF</button></form></body></html>",
        bg_color, temp_color, current_temperature,
        alarm_active ? "<div class=\"alarm\">⚠️ TEMPERATURA ELEVADA! ⚠️</div>\n" : "",
        current_pwm_percent > 0 ? "Ligada" : "Desligada", current_pwm_percent);

    tcp_write(tpcb, html, strlen(html), TCP_WRITE_FLAG_COPY);
    tcp_output(tpcb);
    free(request);
    pbuf_free(p);
    return ERR_OK;
}