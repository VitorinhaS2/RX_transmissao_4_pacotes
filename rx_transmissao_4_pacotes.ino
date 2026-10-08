
// Declaração de bibliotecas 
#include <Arduino.h>
#include "LoRaWan_APP.h"
#include "LoRaConfig.h"
#include "HT_SSD1306Wire.h"
#include "sd_read_write.h"


// CONFIGURAÇÕES
#define TOTAL_PACOTES   2400
#define TAMANHO_PACOTE 255

// Pacote especial enviado pelo TX para finalizar
#define BYTE_FINALIZACAO 0xFF

// ARQUIVOS NO CARTÃO SD

#define ARQUIVO_TXT "/lora_experimento.txt"
#define ARQUIVO_CSV "/lora_experimento.csv"

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


// INICIALIZAÇÃO DO SD

bool inicializarSD()
{
    Serial.println();
    Serial.println("Inicializando cartao SD...");

    sd_spi.begin(SCK, MISO, MOSI, CS);

    if (!SD.begin(CS, sd_spi)){
        Serial.println("ERRO: Card Mount Failed");
        return false;
    }

    uint8_t cardType = SD.cardType();

    if (cardType == CARD_NONE){
        Serial.println("ERRO: Nenhum cartao SD encontrado.");
        return false;
    }

    Serial.print("Tipo do cartao: ");

    if (cardType == CARD_MMC){
        Serial.println("MMC");
    } else if (cardType == CARD_SD) {
        Serial.println("SDSC");
    } else if (cardType == CARD_SDHC) {
        Serial.println("SDHC");
    } else {
        Serial.println("UNKNOWN");
    }

    uint64_t cardSize = SD.cardSize() / (1024 * 1024);

    Serial.print("Tamanho do cartao: ");
    Serial.print(cardSize);
    Serial.println(" MB");

    return true;
}

// função para criar oas arquivos tanto em txt quanto csv pra melhor vizualização dos dados.

bool criarArquivos()
{
    // Remove arquivos antigos

    if (SD.exists(ARQUIVO_TXT)) {
        SD.remove(ARQUIVO_TXT);
    }

    if (SD.exists(ARQUIVO_CSV)) {
        SD.remove(ARQUIVO_CSV);
    }

    // TXT

    arquivoTXT = SD.open(ARQUIVO_TXT, FILE_WRITE);

    if (!arquivoTXT) {
        Serial.println("ERRO ao criar TXT.");
        return false;
    }

    arquivoTXT.println(
        "PACOTE | TEMPO_MS | DADOS | TAMANHO | RSSI | SNR | INTEGRIDADE"
    );

    arquivoTXT.flush();

    // CSV

    arquivoCSV = SD.open(ARQUIVO_CSV, FILE_WRITE);

    if (!arquivoCSV) {
        Serial.println("ERRO ao criar CSV.");
        arquivoTXT.close();
        return false;
    }

    arquivoCSV.println(
        "pacote;tempo_ms;dados_recebidos;tamanho;rssi;snr;integridade"
    );

    arquivoCSV.flush();

    Serial.println("Arquivos criados:");
    Serial.println(ARQUIVO_TXT);
    Serial.println(ARQUIVO_CSV);

    return true;
}


// VERIFICA PACOTE DE FINALIZAÇÃO

bool ehPacoteFinalizacao(
    uint8_t *payload,
    uint16_t size
)
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
//
// O TX envia:
// 00 01 02 03 ... FC FD FE
//
// Portanto:
// payload[0] = 0
// payload[1] = 1
// ...
// payload[254] = 254
//

bool verificarIntegridade(uint8_t *payload, uint16_t size)
{
    // Primeiro verifica o tamanho
    if (size != TAMANHO_PACOTE)
    {
        return false;
    }

    // Depois verifica cada byte
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

void escreverBytesHexTXT(
    File &arquivo,
    uint8_t *payload,
    uint16_t size
)
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

void escreverBytesHexCSV(
    File &arquivo,
    uint8_t *payload,
    uint16_t size
)
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

void salvarPacote(
    uint8_t *payload,
    uint16_t size,
    int16_t rssi,
    int8_t snr
)
{
    if (!sdOK)
    {
        return;
    }


    unsigned long tempoAtual = 0;

    if (experimentoIniciado)
    {
        tempoAtual = millis() - inicioExperimento;
    }


    bool integridadeOK =
        verificarIntegridade(payload, size);


    // CONTROLE DE INTEGRIDADE

    if (size != TAMANHO_PACOTE)
    {
        pacotesInvalidos++;
    }

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

        escreverBytesHexTXT(
            arquivoTXT,
            payload,
            size
        );

        arquivoTXT.print(" | ");

        arquivoTXT.print(size);
        arquivoTXT.print(" | ");

        arquivoTXT.print(rssi);
        arquivoTXT.print(" | ");

        arquivoTXT.print(snr);
        arquivoTXT.print(" | ");

        if (integridadeOK)
        {
            arquivoTXT.println("OK");
        }
        else
        {
            arquivoTXT.println("ERRO");
        }
    }

    // CSV

    if (arquivoCSV)
    {
        arquivoCSV.print(pacotesRecebidos);
        arquivoCSV.print(";");

        arquivoCSV.print(tempoAtual);
        arquivoCSV.print(";");

        escreverBytesHexCSV(
            arquivoCSV,
            payload,
            size
        );

        arquivoCSV.print(";");

        arquivoCSV.print(size);
        arquivoCSV.print(";");

        arquivoCSV.print(rssi);
        arquivoCSV.print(";");

        arquivoCSV.print(snr);
        arquivoCSV.print(";");

        if (integridadeOK) {
            arquivoCSV.println("OK");
        }
        else{
            arquivoCSV.println("ERRO");
        }
    }


    // Grava fisicamente no cartão

    arquivoTXT.flush();
    arquivoCSV.flush();
}


// CALLBACK DE RECEPÇÃO

void OnRxDone(
    uint8_t *payload,
    uint16_t size,
    int16_t rssi,
    int8_t snr
)
{
    // Se já terminou, não processa
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
        Serial.println();


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
        memcpy(
            rxpacket,
            payload,
            TAMANHO_PACOTE
        );


        tamanhoRecebido = size;

        rssiAtual = rssi;
        snrAtual = snr;


        // ----------------------------------------------------
        // Primeiro pacote
        // ----------------------------------------------------

        if (!experimentoIniciado)
        {
            experimentoIniciado = true;

            inicioExperimento = millis();

            Serial.println();
            Serial.println("=================================");
            Serial.println("    EXPERIMENTO INICIADO");
            Serial.println("=================================");
            Serial.println();
        }


        // ----------------------------------------------------
        // Conta pacote
        // ----------------------------------------------------

        pacotesRecebidos++;

        somaRSSI += rssi;
        somaSNR += snr;


        // ----------------------------------------------------
        // Marca pacote recebido
        // ----------------------------------------------------

        pacoteRecebido = true;


        // ----------------------------------------------------
        // Salva no SD
        // ----------------------------------------------------

        salvarPacote(
            payload,
            size,
            rssi,
            snr
        );
    }
    else
    {
        pacotesInvalidos++;

        Serial.print(
            "Pacote com tamanho inesperado: "
        );

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
        percentualPerda =
            ((float)(
                TOTAL_PACOTES -
                pacotesRecebidos
            ) / TOTAL_PACOTES) * 100.0;
    }


    display.clear();


    display.drawString(
        0,
        0,
        "EXPERIMENTO LoRa"
    );


    display.drawString(
        0,
        12,
        "TX: " + String(TOTAL_PACOTES)
    );


    display.drawString(
        0,
        24,
        "RX: " + String(pacotesRecebidos)
    );


    display.drawString(
        0,
        36,
        "Perda: " +
        String(percentualPerda, 2) +
        "%"
    );


    display.drawString(
        0,
        48,
        "RSSI: " +
        String(rssiAtual) +
        " dBm"
    );


    display.display();
}


// RESULTADO FINAL

void mostrarResultadoFinal()
{
    float percentualPerda = 0.0;

    uint32_t pacotesPerdidos = 0;


    if (pacotesRecebidos <= TOTAL_PACOTES)
    {
        pacotesPerdidos =
            TOTAL_PACOTES -
            pacotesRecebidos;

        percentualPerda =
            ((float)pacotesPerdidos /
             TOTAL_PACOTES) * 100.0;
    }


    float rssiMedio = 0.0;
    float snrMedio = 0.0;


    if (pacotesRecebidos > 0)
    {
        rssiMedio =
            (float)somaRSSI /
            pacotesRecebidos;

        snrMedio =
            (float)somaSNR /
            pacotesRecebidos;
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


    display.drawString(
        0,
        0,
        "RESULTADO FINAL"
    );


    display.drawString(
        0,
        12,
        "RX: " +
        String(pacotesRecebidos)
    );


    display.drawString(
        0,
        24,
        "Perdidos: " +
        String(pacotesPerdidos)
    );


    display.drawString(
        0,
        36,
        "Perda: " +
        String(percentualPerda, 2) +
        "%"
    );


    display.drawString(
        0,
        48,
        "RSSI: " +
        String(rssiMedio, 1)
    );


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
    Serial.println();


    // --------------------------------------------------------
    // Inicializa placa
    // --------------------------------------------------------

    Mcu.begin(
        HELTEC_BOARD,
        SLOW_CLK_TPYE
    );


    // --------------------------------------------------------
    // OLED
    // --------------------------------------------------------

    display.init();

    display.clear();

    display.drawString(
        0,
        0,
        "RX LoRa"
    );

    display.drawString(
        0,
        15,
        "Inicializando..."
    );

    display.display();


    // --------------------------------------------------------
    // SD
    // --------------------------------------------------------

    sdOK = inicializarSD();


    if (!sdOK)
    {
        display.clear();

        display.drawString(
            0,
            0,
            "ERRO SD"
        );

        display.drawString(
            0,
            15,
            "Verifique cartao"
        );

        display.display();


        Serial.println();
        Serial.println(
            "ERRO: SD nao inicializado."
        );


        while (true)
        {
            delay(1000);
        }
    }


    // --------------------------------------------------------
    // Cria arquivos
    // --------------------------------------------------------

    if (!criarArquivos())
    {
        display.clear();

        display.drawString(
            0,
            0,
            "ERRO ARQUIVO"
        );

        display.display();


        while (true)
        {
            delay(1000);
        }
    }


    // --------------------------------------------------------
    // Callbacks
    // --------------------------------------------------------

    RadioEvents.RxDone = OnRxDone;
    RadioEvents.RxTimeout = OnRxTimeout;
    RadioEvents.RxError = OnRxError;


    // --------------------------------------------------------
    // Inicializa rádio
    // --------------------------------------------------------

    Radio.Init(&RadioEvents);

    Radio.SetChannel(
        RF_FREQUENCY
    );


    // --------------------------------------------------------
    // Configuração RX
    // --------------------------------------------------------

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


    Serial.println(
        "Radio inicializado."
    );

    Serial.println(
        "SD inicializado."
    );

    Serial.println(
        "Aguardando pacotes..."
    );

    Serial.println();


    display.clear();

    display.drawString(
        0,
        0,
        "RX LoRa"
    );

    display.drawString(
        0,
        15,
        "Aguardando..."
    );

    display.display();


    // --------------------------------------------------------
    // Inicia recepção
    // --------------------------------------------------------

    Radio.Rx(0);
}


// LOOP

void loop()
{
    // --------------------------------------------------------
    // Processa rádio
    // --------------------------------------------------------

    Radio.IrqProcess();


    // --------------------------------------------------------
    // Atualiza display
    // --------------------------------------------------------

    static unsigned long ultimoDisplay = 0;


    if (millis() - ultimoDisplay >= 250)
    {
        ultimoDisplay = millis();


        if (
            experimentoIniciado &&
            !experimentoFinalizado
        )
        {
            atualizarDisplay();
        }
    }


    // --------------------------------------------------------
    // Informações no Serial
    // --------------------------------------------------------

    if (pacoteRecebido)
    {
        pacoteRecebido = false;


        if (pacotesRecebidos % 10 == 0)
        {
            bool integridadeOK =
                verificarIntegridade(
                    (uint8_t *)rxpacket,
                    tamanhoRecebido
                );


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