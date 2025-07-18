# 🌡️ Projeto: Monitor de Temperatura com Wi-Fi

Este projeto desenvolvido por alunos da **UNIVASF** consiste em um sistema embarcado utilizando a **Raspberry Pi Pico W** para **monitoramento de temperatura ambiente** com controle de um **Cooler Fan via PWM** e **interface web integrada via Wi-Fi**.

## 🔧 Funcionalidades

- Leitura de temperatura através de sensor NTC conectado ao ADC.
- Cálculo da temperatura em Celsius utilizando a equação de resistência térmica.
- Controle do duty cycle de um cooler (ventoinha) proporcional à temperatura (via PWM).
- Ativação de alarme em caso de temperatura crítica (> 60 °C).
- Interface web simples exibindo a temperatura atual, status do cooler e botão para ligar/desligar um LED indicador via Wi-Fi.
- Atualização automática da página web a cada 3 segundos.

## 📡 Tecnologias Utilizadas

- **Raspberry Pi Pico W**
- **Linguagem C**
- **FreeRTOS & LWIP (stack TCP/IP)**
- **PWM / ADC integrados**
- **Servidor HTTP básico (porta 80)**

## 🖧 Requisitos

- Ambiente de desenvolvimento para Raspberry Pi Pico (SDK e toolchain C).
- Conexão Wi-Fi disponível (SSID e senha devem ser configurados no código).
- Navegador web para acessar a interface do sistema (via IP local).

## 👨‍💻 Autores

- Caio  
- Felipe  
- Osvaldo  
- Domingos

> Universidade Federal do Vale do São Francisco - UNIVASF  
> Data: 16/07/2025

## 📸 Exemplo de Interface Web

A interface HTML gerada apresenta:

- Temperatura em tempo real (com cores indicativas)
- Alerta visual em caso de alarme
- Estado da ventoinha e porcentagem de PWM
- Botões para ativar/desativar LED via comandos HTTP

---

