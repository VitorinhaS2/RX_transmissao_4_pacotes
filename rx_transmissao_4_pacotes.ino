// Declaração de bibliotecas
#include <Arduino.h>
#include "LoRaWan_APP.h"
#include "LoRaConfig.h"
#include "HT_SSD1306Wire.h"
#include "sd_read_write.h"

// CONFIGURAÇÕES
#define TOTAL_PACOTES 2400
#define TAMANHO_PACOTE 255
#define BYTE_FINALIZACAO 0xFF

// OLED
static SSD1306Wire display(0x3C, 500000, SDA_OLED, SCL_OLED, GEOMETRY_128_64, RST_OLED);

// RÁDIO
static RadioEvents_t RadioEvents;

// CARTÃO SD
SPIClass sd_spi(HSPI);
File arquivoTXT;
File arquivoCSV;
bool sdOK = false;

// VARIÁVEIS DO EXPERIMENTO
uint32_t pacotesRecebidos = 0;
uint32_t pacotesInvalidos = 0;
uint32_t pacotesIntegridadeOK = 0;
uint32_t pacotesIntegridadeERRO = 0;
int16_t rssiAtual = 0;
int32_t somaRSSI = 0;
int8_t snrAtual = 0;
int32_t somaSNR = 0;
uint16_t tamanhoRecebido = 0;

// CONTROLE
bool pacoteRecebido = false;
bool experimentoIniciado = false;
bool experimentoFinalizado = false;
unsigned long inicioExperimento = 0;

// Número do experimento atual
// A cada reinicialização procura o próximo número disponível
int numeroExperimento = 0;

// Função pra ligar o display OLED da Heltec
void VextON()
{
    pinMode(Vext, OUTPUT);
    digitalWrite(Vext, LOW);
}

void VextOFF()
{
    pinMode(Vext, OUTPUT);
    digitalWrite(Vext, HIGH);
}

// CRIA ARQUIVOS
// Cria um novo par de arquivos sem apagar os experimentos anteriores
bool criarArquivos()
{
    char nomeTXT[40];
    char nomeCSV[40];

    numeroExperimento = 1;

    // Procura o primeiro número de experimento que ainda não existe
    while (true)
    {
        sprintf(nomeTXT, "/lora_experimento_%03d.txt", numeroExperimento);
        sprintf(nomeCSV, "/lora_experimento_%03d.csv", numeroExperimento);

        if (!SD.exists(nomeTXT) && !SD.exists(nomeCSV))
        {
            break;
        }

        numeroExperimento++;
    }

    // Cria o arquivo TXT
    arquivoTXT = SD.open(nomeTXT, FILE_WRITE);

    if (!arquivoTXT)
    {
        Serial.println("ERRO ao criar TXT.");
        return false;
    }

    arquivoTXT.println("PACOTE | TEMPO_MS | DADOS | TAMANHO | RSSI | SNR | INTEGRIDADE");

    // Cria o arquivo CSV
    arquivoCSV = SD.open(nomeCSV, FILE_WRITE);

    if (!arquivoCSV)
    {
        Serial.println("ERRO ao criar CSV.");
        arquivoTXT.close();
        return false;
    }

    arquivoCSV.println("pacote;tempo_ms;dados_recebidos;tamanho;rssi;snr;integridade");

    // Garante que os cabeçalhos sejam gravados no cartão
    arquivoTXT.flush();
    arquivoCSV.flush();

    Serial.println("Arquivos criados:");
    Serial.println(nomeTXT);
    Serial.println(nomeCSV);

    return true;
}

// VERIFICA PACOTE DE FINALIZAÇÃO
// O pacote de finalização possui 255 bytes iguais a FF
bool ehPacoteFinalizacao(uint8_t *payload, uint16_t size)
{
    if (size != TAMANHO_PACOTE)
    {
        return false;
    }

    for (uint16_t i = 0; i < TAMANHO_PACOTE; i++)
    {
        if (payload[i] != BYTE_FINALIZACAO)
        {
            return false;
        }
    }

    return true;
}

// VERIFICA INTEGRIDADE DO PACOTE
// O TX envia: 00 01 02 03 ... FC FD FE
// payload[0] = 0, payload[1] = 1, ... payload[254] = 254
bool verificarIntegridade(uint8_t *payload, uint16_t size)
{
    if (size != TAMANHO_PACOTE)
    {
        return false;
    }

    for (uint16_t i = 0; i < TAMANHO_PACOTE; i++)
    {
        if (payload[i] != (uint8_t)i)
        {
            return false;
        }
    }

    return true;
}

// ESCREVE OS BYTES EM HEXADECIMAL
void escreverBytesHexTXT(File &arquivo, uint8_t *payload, uint16_t size)
{
    for (uint16_t i = 0; i < size; i++)
    {
        if (payload[i] < 0x10)
        {
            arquivo.print("0");
        }

        arquivo.print(payload[i], HEX);

        if (i < size - 1)
        {
            arquivo.print(" ");
        }
    }
}

// ESCREVE OS BYTES HEX NO CSV
void escreverBytesHexCSV(File &arquivo, uint8_t *payload, uint16_t size)
{
    for (uint16_t i = 0; i < size; i++)
    {
        if (payload[i] < 0x10)
        {
            arquivo.print("0");
        }

        arquivo.print(payload[i], HEX);

        if (i < size - 1)
        {
            arquivo.print(" ");
        }
    }
}

// SALVA PACOTE NOS DOIS ARQUIVOS
void salvarPacote(uint8_t *payload, uint16_t size, int16_t rssi, int8_t snr)
{
    if (!sdOK)
    {
        return;
    }

    unsigned long tempoAtual = millis() - inicioExperimento;
    bool integridadeOK = verificarIntegridade(payload, size);

    // CONTROLE DE INTEGRIDADE
    if (integridadeOK)
    {
        pacotesIntegridadeOK++;
    }
    else
    {
        pacotesIntegridadeERRO++;
    }

    // TXT
    if (arquivoTXT)
    {
        arquivoTXT.print(pacotesRecebidos);
        arquivoTXT.print(" | ");
        arquivoTXT.print(tempoAtual);
        arquivoTXT.print(" | ");

        escreverBytesHexTXT(arquivoTXT, payload, size);

        arquivoTXT.print(" | ");
        arquivoTXT.print(size);
        arquivoTXT.print(" | ");
        arquivoTXT.print(rssi);
        arquivoTXT.print(" | ");
        arquivoTXT.print(snr);
        arquivoTXT.print(" | ");
        arquivoTXT.println(integridadeOK ? "OK" : "ERRO");
    }

    // CSV
    if (arquivoCSV)
    {
        arquivoCSV.print(pacotesRecebidos);
        arquivoCSV.print(";");
        arquivoCSV.print(tempoAtual);
        arquivoCSV.print(";");

        escreverBytesHexCSV(arquivoCSV, payload, size);

        arquivoCSV.print(";");
        arquivoCSV.print(size);
        arquivoCSV.print(";");
        arquivoCSV.print(rssi);
        arquivoCSV.print(";");
        arquivoCSV.print(snr);
        arquivoCSV.print(";");
        arquivoCSV.println(integridadeOK ? "OK" : "ERRO");
    }

    // O flush não é feito a cada pacote porque isso aumenta o tempo de gravação no cartão SD.
    // A cada 10 pacotes os dados são forçados para o cartão.
    if (pacotesRecebidos % 10 == 0)
    {
        if (arquivoTXT) arquivoTXT.flush();
        if (arquivoCSV) arquivoCSV.flush();
    }
}

// CALLBACK DE RECEPÇÃO
void OnRxDone(uint8_t *payload, uint16_t size, int16_t rssi, int8_t snr)
{
    // Se já terminou, não processa mais pacotes
    if (experimentoFinalizado)
    {
        return;
    }

    // PACOTE DE FINALIZAÇÃO
    if (ehPacoteFinalizacao(payload, size))
    {
        rssiAtual = rssi;
        snrAtual = snr;

        Serial.println();
        Serial.println("=================================");
        Serial.println("    FINALIZACAO RECEBIDA");
        Serial.println("=================================");

        // Fecha os arquivos antes de finalizar
        if (arquivoTXT)
        {
            arquivoTXT.flush();
            arquivoTXT.close();
        }

        if (arquivoCSV)
        {
            arquivoCSV.flush();
            arquivoCSV.close();
        }

        Radio.Sleep();
        experimentoFinalizado = true;
        mostrarResultadoFinal();
        return;
    }

    // PACOTE NORMAL
    if (size == TAMANHO_PACOTE)
    {
        // Copia os dados para o buffer global
        memcpy(rxpacket, payload, TAMANHO_PACOTE);

        tamanhoRecebido = size;
        rssiAtual = rssi;
        snrAtual = snr;

        // Primeiro pacote
        if (!experimentoIniciado)
        {
            experimentoIniciado = true;
            inicioExperimento = millis();

            Serial.println();
            Serial.println("=================================");
            Serial.println("    EXPERIMENTO INICIADO");
            Serial.println("=================================");
        }

        // Conta pacote e atualiza RSSI/SNR
        pacotesRecebidos++;
        somaRSSI += rssi;
        somaSNR += snr;

        // Marca pacote recebido
        pacoteRecebido = true;

        // Salva no SD
        salvarPacote(payload, size, rssi, snr);
    }
    else
    {
        pacotesInvalidos++;

        Serial.print("Pacote com tamanho inesperado: ");
        Serial.println(size);
    }

    // CONTINUA ESCUTANDO
    if (!experimentoFinalizado)
    {
        Radio.Rx(0);
    }
}

// TIMEOUT
void OnRxTimeout(void)
{
    if (!experimentoFinalizado)
    {
        Radio.Rx(0);
    }
}

// ERRO
void OnRxError(void)
{
    if (!experimentoFinalizado)
    {
        Radio.Rx(0);
    }
}

// DISPLAY
void atualizarDisplay()
{
    float percentualPerda = 0.0;

    if (pacotesRecebidos <= TOTAL_PACOTES)
    {
        percentualPerda = ((float)(TOTAL_PACOTES - pacotesRecebidos) / TOTAL_PACOTES) * 100.0;
    }

    display.clear();
    display.setFont(ArialMT_Plain_10);
    display.setTextAlignment(TEXT_ALIGN_LEFT);

    display.drawString(0, 0, "EXPERIMENTO LoRa");
    display.drawString(0, 12, "TX: " + String(TOTAL_PACOTES));
    display.drawString(0, 24, "RX: " + String(pacotesRecebidos));
    display.drawString(0, 36, "Perda: " + String(percentualPerda, 2) + "%");
    display.drawString(0, 48, "RSSI: " + String(rssiAtual) + " dBm");

    display.display();
}

// RESULTADO FINAL
void mostrarResultadoFinal()
{
    uint32_t pacotesPerdidos = 0;
    float percentualPerda = 0.0;
    float rssiMedio = 0.0;
    float snrMedio = 0.0;

    if (pacotesRecebidos <= TOTAL_PACOTES)
    {
        pacotesPerdidos = TOTAL_PACOTES - pacotesRecebidos;
        percentualPerda = ((float)pacotesPerdidos / TOTAL_PACOTES) * 100.0;
    }

    if (pacotesRecebidos > 0)
    {
        rssiMedio = (float)somaRSSI / pacotesRecebidos;
        snrMedio = (float)somaSNR / pacotesRecebidos;
    }

    // SERIAL
    Serial.println();
    Serial.println("=================================");
    Serial.println("       RESULTADO FINAL");
    Serial.println("=================================");

    Serial.print("Pacotes esperados: ");
    Serial.println(TOTAL_PACOTES);

    Serial.print("Pacotes recebidos: ");
    Serial.println(pacotesRecebidos);

    Serial.print("Pacotes perdidos: ");
    Serial.println(pacotesPerdidos);

    Serial.print("Percentual de perda: ");
    Serial.print(percentualPerda, 2);
    Serial.println("%");

    Serial.print("RSSI medio: ");
    Serial.print(rssiMedio, 2);
    Serial.println(" dBm");

    Serial.print("SNR medio: ");
    Serial.print(snrMedio, 2);
    Serial.println(" dB");

    Serial.print("Ultimo RSSI: ");
    Serial.print(rssiAtual);
    Serial.println(" dBm");

    Serial.print("Pacotes tamanho incorreto: ");
    Serial.println(pacotesInvalidos);

    Serial.print("Integridade OK: ");
    Serial.println(pacotesIntegridadeOK);

    Serial.print("Integridade ERRO: ");
    Serial.println(pacotesIntegridadeERRO);

    Serial.println("=================================");

    // OLED
    display.clear();
    display.setFont(ArialMT_Plain_10);
    display.setTextAlignment(TEXT_ALIGN_LEFT);

    display.drawString(0, 0, "RESULTADO FINAL");
    display.drawString(0, 12, "RX: " + String(pacotesRecebidos));
    display.drawString(0, 24, "Perdidos: " + String(pacotesPerdidos));
    display.drawString(0, 36, "Perda: " + String(percentualPerda, 2) + "%");
    display.drawString(0, 48, "RSSI: " + String(rssiMedio, 1));

    display.display();
}

// SETUP
void setup()
{
    Serial.begin(115200);
    delay(1000);

    Serial.println();
    Serial.println("=================================");
    Serial.println("       RX LoRa - EXPERIMENTO");
    Serial.println("=================================");

    // Inicializa placa
    Mcu.begin(HELTEC_BOARD, SLOW_CLK_TPYE);

    // OLED
    VextON();
    delay(200);

    display.init();
    display.clear();
    display.setFont(ArialMT_Plain_10);
    display.setTextAlignment(TEXT_ALIGN_LEFT);
    display.drawString(0, 0, "RX LoRa");
    display.drawString(0, 15, "Inicializando...");
    display.display();

    // SD
    sd_spi.begin(SCK, MISO, MOSI, CS);

    if (!SD.begin(CS, sd_spi) || SD.cardType() == CARD_NONE)
    {
        sdOK = false;

        display.clear();
        display.drawString(0, 0, "ERRO SD");
        display.drawString(0, 15, "Verifique cartao");
        display.display();

        Serial.println("ERRO: SD nao inicializado.");

        while (true)
        {
            delay(1000);
        }
    }

    sdOK = true;

    // Cria arquivos
    if (!criarArquivos())
    {
        display.clear();
        display.drawString(0, 0, "ERRO ARQUIVO");
        display.display();

        while (true)
        {
            delay(1000);
        }
    }

    // Callbacks
    RadioEvents.RxDone = OnRxDone;
    RadioEvents.RxTimeout = OnRxTimeout;
    RadioEvents.RxError = OnRxError;

    // Inicializa rádio
    Radio.Init(&RadioEvents);
    Radio.SetChannel(RF_FREQUENCY);

    // Configuração RX
    Radio.SetRxConfig(
        MODEM_LORA,
        LORA_BANDWIDTH,
        LORA_SPREADING_FACTOR,
        LORA_CODINGRATE,
        0,
        LORA_PREAMBLE_LENGTH,
        LORA_SYMBOL_TIMEOUT,
        LORA_FIX_LENGTH_PAYLOAD_ON,
        0,
        true,
        0,
        0,
        LORA_IQ_INVERSION_ON,
        true
    );

    Serial.println("Radio inicializado.");
    Serial.println("SD inicializado.");
    Serial.println("Aguardando pacotes...");

    display.clear();
    display.drawString(0, 0, "RX LoRa");
    display.drawString(0, 15, "Aguardando...");
    display.display();

    // Inicia recepção
    Radio.Rx(0);
}

// LOOP
void loop()
{
    // Processa rádio
    Radio.IrqProcess();

    // Atualiza display
    static unsigned long ultimoDisplay = 0;

    if (millis() - ultimoDisplay >= 250)
    {
        ultimoDisplay = millis();

        if (experimentoIniciado && !experimentoFinalizado)
        {
            atualizarDisplay();
        }
    }

    // Informações no Serial
    if (pacoteRecebido)
    {
        pacoteRecebido = false;

        if (pacotesRecebidos % 10 == 0)
        {
            bool integridadeOK = verificarIntegridade((uint8_t *)rxpacket, tamanhoRecebido);

            Serial.print("Recebidos: ");
            Serial.print(pacotesRecebidos);
            Serial.print(" | Tamanho: ");
            Serial.print(tamanhoRecebido);
            Serial.print(" | RSSI: ");
            Serial.print(rssiAtual);
            Serial.print(" dBm");
            Serial.print(" | SNR: ");
            Serial.print(snrAtual);
            Serial.print(" dB");
            Serial.print(" | Integridade: ");

            if (integridadeOK)
            {
                Serial.println("OK");
            }
            else
            {
                Serial.println("ERRO");
            }
        }
    }
}