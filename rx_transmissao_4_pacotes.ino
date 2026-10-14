// Declaracao de bibliotecas
#include <Arduino.h>
#include <string.h>
#include "LoRaWan_APP.h"
#include "LoRaConfig.h"
#include "HT_SSD1306Wire.h"
#include "sd_read_write.h"

// CONFIGURACOES
#define TOTAL_PACOTES 2400
#define TAMANHO_PACOTE 255
#define BYTE_FINALIZACAO 0xFF

#define TAMANHO_PING 5
#define TAMANHO_PONG 5
#define TAMANHO_START 8
#define TAMANHO_ACK_START 12

// OLED
static SSD1306Wire display(0x3C, 500000, SDA_OLED, SCL_OLED, GEOMETRY_128_64, RST_OLED);

// RADIO
static RadioEvents_t RadioEvents;

// CARTAO SD
SPIClass sd_spi(HSPI);

File arquivoTXT;
File arquivoCSV;

bool sdOK = false;

// BUFFERS DE CONTROLE
uint8_t bufferPong[TAMANHO_PONG] = {
    'P', 'O', 'N', 'G', 0x01
};

uint8_t bufferAck[TAMANHO_ACK_START];

bool respondendoControle = false;

// ============================================================
// VARIAVEIS DO EXPERIMENTO
// ============================================================

uint32_t numeroExperimento = 0;
uint32_t nonceSessao = 0;

bool sessaoAtiva = false;
bool experimentoIniciado = false;
bool experimentoFinalizado = false;
bool pacoteRecebido = false;

uint32_t pacotesRecebidos = 0;
uint32_t pacotesRecebidosTotal = 0;
uint32_t pacotesInvalidos = 0;
uint32_t pacotesIntegridadeOK = 0;
uint32_t pacotesIntegridadeERRO = 0;

int16_t rssiAtual = 0;
int32_t somaRSSI = 0;

int8_t snrAtual = 0;
int32_t somaSNR = 0;

uint16_t tamanhoRecebido = 0;
unsigned long inicioExperimento = 0;

uint8_t ultimoPayload[TAMANHO_PACOTE];

// ============================================================
// PROTOTIPOS
// ============================================================

void mostrarResultadoFinal();
void atualizarDisplay();
void fecharArquivos();
void enviarControle(uint8_t *payload, uint16_t tamanho);

// ============================================================
// FUNCOES AUXILIARES
// ============================================================

void VextON()
{
    pinMode(Vext, OUTPUT);
    digitalWrite(Vext, LOW);
}

// Le um valor de 32 bits armazenado em quatro bytes
uint32_t lerU32(const uint8_t *p)
{
    return (uint32_t)p[0] |
           ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) |
           ((uint32_t)p[3] << 24);
}
// Grava um valor de 32 bits em quatro bytes
void gravarU32(uint8_t *p, uint32_t valor)
{
    p[0] = valor & 0xFF;
    p[1] = (valor >> 8) & 0xFF;
    p[2] = (valor >> 16) & 0xFF;
    p[3] = (valor >> 24) & 0xFF;
}


// VERIFICACAO DAS MENSAGENS DE CONTROLE
// Verifica se a mensagem recebida e um PING valido
bool ehPing(uint8_t *payload, uint16_t tamanho)
{
    return tamanho == TAMANHO_PING &&
           payload[0] == 'P' &&
           payload[1] == 'I' &&
           payload[2] == 'N' &&
           payload[3] == 'G' &&
           payload[4] == 0x01;
}

// Verifica se a mensagem recebida solicita o inicio do experimento
bool ehStart(uint8_t *payload, uint16_t tamanho)
{
    return tamanho == TAMANHO_START &&
           payload[0] == 'S' &&
           payload[1] == 'T' &&
           payload[2] == 'R' &&
           payload[3] == 'T';
}

// VERIFICACAO DO PACOTE DE FINALIZACAO

// O pacote de finalizacao possui 255 bytes iguais a FF
bool ehPacoteFinalizacao(uint8_t *payload, uint16_t tamanho)
{
    if (tamanho != TAMANHO_PACOTE)
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

// ============================================================
// VERIFICACAO DE INTEGRIDADE
// ============================================================

// Todo pacote normal deve conter os valores 0 a 254.
// O valor de cada byte deve ser igual ao indice da coluna.
bool verificarIntegridade(uint8_t *payload, uint16_t tamanho)
{
    if (tamanho != TAMANHO_PACOTE)
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

// ============================================================
// FECHA OS ARQUIVOS DO EXPERIMENTO
// ============================================================

void fecharArquivos()
{
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
}

// ============================================================
// CRIA ARQUIVOS
// ============================================================

bool criarArquivos()
{
    char nomeTXT[48];
    char nomeCSV[48];

    numeroExperimento = 1;

    while (true)
    {
        snprintf(nomeTXT, sizeof(nomeTXT),
                 "/lora_experimento_%03lu.txt",
                 (unsigned long)numeroExperimento);

        snprintf(nomeCSV, sizeof(nomeCSV),
                 "/lora_experimento_%03lu.csv",
                 (unsigned long)numeroExperimento);

        if (!SD.exists(nomeTXT) && !SD.exists(nomeCSV))
        {
            break;
        }

        numeroExperimento++;
    }

    arquivoTXT = SD.open(nomeTXT, FILE_WRITE);

    if (!arquivoTXT)
    {
        Serial.println("ERRO ao criar TXT.");
        return false;
    }

    arquivoTXT.println(
        "TEMPO_MS | DADOS_HEX | TAMANHO | RSSI | SNR | INTEGRIDADE");

    arquivoCSV = SD.open(nomeCSV, FILE_WRITE);

    if (!arquivoCSV)
    {
        Serial.println("ERRO ao criar CSV.");
        arquivoTXT.close();
        return false;
    }

    // Nao ha colunas de recepcao ou sequencia.
    // Cada linha de dados representa um pacote recebido.
    arquivoCSV.print("experimento;tempo_ms;tamanho;rssi;snr;integridade");

    for (uint16_t i = 0; i < TAMANHO_PACOTE; i++)
    {
        char coluna[16];
        snprintf(coluna, sizeof(coluna), ";byte_%03u", i);
        arquivoCSV.print(coluna);
    }

    arquivoCSV.println();

    arquivoTXT.flush();
    arquivoCSV.flush();

    Serial.println();
    Serial.println("Arquivos criados:");
    Serial.println(nomeTXT);
    Serial.println(nomeCSV);

    return true;
}

// ============================================================
// ESCREVE OS BYTES EM HEXADECIMAL NO TXT
// ============================================================

void escreverBytesHexTXT(File &arquivo,
                         uint8_t *payload,
                         uint16_t tamanho)
{
    for (uint16_t i = 0; i < tamanho; i++)
    {
        if (payload[i] < 0x10)
        {
            arquivo.print("0");
        }

        arquivo.print(payload[i], HEX);

        if (i < tamanho - 1)
        {
            arquivo.print(" ");
        }
    }
}

// ============================================================
// SALVA PACOTE NOS ARQUIVOS
// ============================================================

void salvarPacote(uint8_t *payload,
                  uint16_t tamanho,
                  int16_t rssi,
                  int8_t snr)
{
    if (!sdOK || !arquivoTXT || !arquivoCSV)
    {
        return;
    }

    unsigned long tempoAtual = millis() - inicioExperimento;

    bool integridadeOK = verificarIntegridade(payload, tamanho);

    if (integridadeOK)
    {
        pacotesIntegridadeOK++;
    }
    else
    {
        pacotesIntegridadeERRO++;
    }

    // --------------------------------------------------------
    // SALVAMENTO TXT
    // --------------------------------------------------------

    arquivoTXT.print(tempoAtual);
    arquivoTXT.print(" | ");

    escreverBytesHexTXT(arquivoTXT, payload, tamanho);

    arquivoTXT.print(" | ");
    arquivoTXT.print(tamanho);
    arquivoTXT.print(" | ");
    arquivoTXT.print(rssi);
    arquivoTXT.print(" | ");
    arquivoTXT.print(snr);
    arquivoTXT.print(" | ");
    arquivoTXT.println(integridadeOK ? "OK" : "ERRO");

    // --------------------------------------------------------
    // SALVAMENTO CSV
    // --------------------------------------------------------

    arquivoCSV.print(numeroExperimento);
    arquivoCSV.print(";");

    arquivoCSV.print(tempoAtual);
    arquivoCSV.print(";");

    arquivoCSV.print(tamanho);
    arquivoCSV.print(";");

    arquivoCSV.print(rssi);
    arquivoCSV.print(";");

    arquivoCSV.print(snr);
    arquivoCSV.print(";");

    arquivoCSV.print(integridadeOK ? "OK" : "ERRO");

    // Cada byte e salvo em decimal na sua propria coluna.
    // Pacotes normais validos sempre terao 0, 1, ..., 254.
    for (uint16_t i = 0; i < TAMANHO_PACOTE; i++)
    {
        arquivoCSV.print(";");

        if (i < tamanho)
        {
            arquivoCSV.print((unsigned int)payload[i]);
        }
    }

    arquivoCSV.println();

    // Grava no cartao a cada 10 pacotes recebidos.
    if (pacotesRecebidosTotal % 10 == 0)
    {
        arquivoTXT.flush();
        arquivoCSV.flush();
    }
}

// ============================================================
// ENVIA RESPOSTA DE CONTROLE
// ============================================================

void enviarControle(uint8_t *payload, uint16_t tamanho)
{
    Radio.Sleep();

    respondendoControle = true;
    Radio.Send(payload, tamanho);
}

// ============================================================
// ENVIA ACK DE INICIO
// ============================================================

void enviarAckStart(uint32_t nonce)
{
    bufferAck[0] = 'A';
    bufferAck[1] = 'C';
    bufferAck[2] = 'K';
    bufferAck[3] = 'S';

    gravarU32(&bufferAck[4], nonce);
    gravarU32(&bufferAck[8], numeroExperimento);

    enviarControle(bufferAck, TAMANHO_ACK_START);
}

// ============================================================
// INICIA UMA NOVA SESSAO
// ============================================================

void iniciarNovaSessao(uint32_t nonce)
{
    fecharArquivos();

    pacotesRecebidos = 0;
    pacotesRecebidosTotal = 0;
    pacotesInvalidos = 0;
    pacotesIntegridadeOK = 0;
    pacotesIntegridadeERRO = 0;

    somaRSSI = 0;
    somaSNR = 0;

    rssiAtual = 0;
    snrAtual = 0;
    tamanhoRecebido = 0;

    experimentoFinalizado = false;
    experimentoIniciado = true;
    sessaoAtiva = true;

    nonceSessao = nonce;
    inicioExperimento = millis();

    if (!criarArquivos())
    {
        sessaoAtiva = false;
        Serial.println("ERRO ao criar arquivos da sessao.");
        Radio.Rx(0);
        return;
    }

    Serial.println();
    Serial.println("=================================");
    Serial.println("      EXPERIMENTO INICIADO");
    Serial.println("=================================");
    Serial.print("Numero do experimento: ");
    Serial.println(numeroExperimento);
    Serial.println("Pacotes esperados: 2400");
    Serial.println("Conteudo esperado: bytes de 0 a 254");

    atualizarDisplay();
    enviarAckStart(nonceSessao);
}

// ============================================================
// CALLBACK: TRANSMISSAO CONCLUIDA
// ============================================================

void OnTxDone(void)
{
    if (respondendoControle)
    {
        respondendoControle = false;
        Radio.Rx(0);
    }
}

// ============================================================
// CALLBACK: TIMEOUT DE TRANSMISSAO
// ============================================================

void OnTxTimeout(void)
{
    if (respondendoControle)
    {
        respondendoControle = false;
        Radio.Rx(0);
    }
}

// ============================================================
// CALLBACK: RECEPCAO
// ============================================================

void OnRxDone(uint8_t *payload,
              uint16_t tamanho,
              int16_t rssi,
              int8_t snr)
{
    // --------------------------------------------------------
    // PING/PONG
    // --------------------------------------------------------

    if (ehPing(payload, tamanho))
    {
        Serial.println("PING recebido. Enviando PONG.");
        enviarControle(bufferPong, TAMANHO_PONG);
        return;
    }

    // --------------------------------------------------------
    // SOLICITACAO DE INICIO
    // --------------------------------------------------------

    if (ehStart(payload, tamanho))
    {
        uint32_t nonce = lerU32(&payload[4]);

        // Reenvia o ACK se for uma repeticao da mesma solicitacao.
        if (sessaoAtiva && nonce == nonceSessao)
        {
            enviarAckStart(nonceSessao);
        }
        else
        {
            iniciarNovaSessao(nonce);
        }

        return;
    }

    // Ignora dados se nao houver experimento ativo.
    if (!sessaoAtiva || experimentoFinalizado)
    {
        Radio.Rx(0);
        return;
    }

    // --------------------------------------------------------
    // PACOTE DE FINALIZACAO
    // --------------------------------------------------------

    if (ehPacoteFinalizacao(payload, tamanho))
    {
        rssiAtual = rssi;
        snrAtual = snr;

        Serial.println();
        Serial.println("=================================");
        Serial.println("    FINALIZACAO RECEBIDA");
        Serial.println("=================================");

        fecharArquivos();
        experimentoFinalizado = true;

        mostrarResultadoFinal();

        // Permanece ligado para o proximo experimento.
        Radio.Rx(0);
        return;
    }

    // --------------------------------------------------------
    // PACOTE NORMAL DE DADOS
    // --------------------------------------------------------

    if (tamanho == TAMANHO_PACOTE)
    {
        memcpy(ultimoPayload, payload, TAMANHO_PACOTE);

        tamanhoRecebido = tamanho;
        rssiAtual = rssi;
        snrAtual = snr;

        // Conta cada pacote de dados recebido, mesmo se invalido.
        pacotesRecebidosTotal++;

        bool integridadeOK = verificarIntegridade(payload, tamanho);

        if (integridadeOK)
        {
            pacotesRecebidos++;
            somaRSSI += rssi;
            somaSNR += snr;
        }
        else
        {
            pacotesInvalidos++;
        }

        pacoteRecebido = true;

        // Salva cada pacote recebido em uma nova linha.
        salvarPacote(payload, tamanho, rssi, snr);
    }
    else
    {
        pacotesInvalidos++;

        Serial.print("Pacote com tamanho inesperado: ");
        Serial.println(tamanho);
    }

    Radio.Rx(0);
}

// ============================================================
// CALLBACKS DE TIMEOUT E ERRO DE RECEPCAO
// ============================================================

void OnRxTimeout(void)
{
    if (!respondendoControle)
    {
        Radio.Rx(0);
    }
}

void OnRxError(void)
{
    if (!respondendoControle)
    {
        Radio.Rx(0);
    }
}

// ============================================================
// DISPLAY
// ============================================================

void atualizarDisplay()
{
    uint32_t recebidosParaPerda = pacotesRecebidos;

    float percentualPerda = 100.0f *
        (TOTAL_PACOTES - min(recebidosParaPerda,
                             (uint32_t)TOTAL_PACOTES)) /
        TOTAL_PACOTES;

    display.clear();
    display.setFont(ArialMT_Plain_10);
    display.setTextAlignment(TEXT_ALIGN_LEFT);

    display.drawString(0, 0,
        "RX - EXP " + String(numeroExperimento));

    display.drawString(0, 12,
        "Validos: " + String(pacotesRecebidos) +
        "/" + String(TOTAL_PACOTES));

    display.drawString(0, 24,
        "Perda estimada: " + String(percentualPerda, 2) + "%");

    display.drawString(0, 36,
        "RSSI: " + String(rssiAtual) + " dBm");

    display.drawString(0, 48,
        "SNR: " + String(snrAtual) + " dB");

    display.display();
}

// ============================================================
// RESULTADO FINAL
// ============================================================

void mostrarResultadoFinal()
{
    uint32_t pacotesPerdidos = 0;
    float percentualPerda = 0.0;
    float rssiMedio = 0.0;
    float snrMedio = 0.0;

    // Sem sequencia, a perda e estimada pela diferenca entre
    // pacotes validos recebidos e pacotes esperados.
    if (pacotesRecebidos < TOTAL_PACOTES)
    {
        pacotesPerdidos = TOTAL_PACOTES - pacotesRecebidos;
    }

    percentualPerda =
        100.0f * pacotesPerdidos / TOTAL_PACOTES;

    if (pacotesRecebidos > 0)
    {
        rssiMedio = (float)somaRSSI / pacotesRecebidos;
        snrMedio = (float)somaSNR / pacotesRecebidos;
    }

    Serial.println();
    Serial.println("=================================");
    Serial.println("          RESULTADO FINAL");
    Serial.println("=================================");

    Serial.print("Experimento: ");
    Serial.println(numeroExperimento);

    Serial.print("Pacotes esperados: ");
    Serial.println(TOTAL_PACOTES);

    Serial.print("Pacotes validos recebidos: ");
    Serial.println(pacotesRecebidos);

    Serial.print("Total de pacotes de dados recebidos: ");
    Serial.println(pacotesRecebidosTotal);

    Serial.print("Pacotes perdidos estimados: ");
    Serial.println(pacotesPerdidos);

    Serial.print("Percentual de perda estimado: ");
    Serial.print(percentualPerda, 2);
    Serial.println("%");

    Serial.print("Pacotes invalidos: ");
    Serial.println(pacotesInvalidos);

    Serial.print("Integridade OK: ");
    Serial.println(pacotesIntegridadeOK);

    Serial.print("Integridade ERRO: ");
    Serial.println(pacotesIntegridadeERRO);

    Serial.print("RSSI medio: ");
    Serial.print(rssiMedio, 2);
    Serial.println(" dBm");

    Serial.print("SNR medio: ");
    Serial.print(snrMedio, 2);
    Serial.println(" dB");

    Serial.println("=================================");

    display.clear();
    display.setFont(ArialMT_Plain_10);
    display.setTextAlignment(TEXT_ALIGN_LEFT);

    display.drawString(0, 0,
        "EXP " + String(numeroExperimento) + " FINALIZADO");

    display.drawString(0, 12,
        "Validos: " + String(pacotesRecebidos));

    display.drawString(0, 24,
        "Perdidos*: " + String(pacotesPerdidos));

    display.drawString(0, 36,
        "Perda*: " + String(percentualPerda, 2) + "%");

    display.drawString(0, 48,
        "RSSI medio: " + String(rssiMedio, 1));

    display.display();
}

// ============================================================
// SETUP
// ============================================================

void setup()
{
    Serial.begin(115200);
    delay(1000);

    Serial.println();
    Serial.println("=================================");
    Serial.println("       RX LoRa - EXPERIMENTO");
    Serial.println("=================================");

    Mcu.begin(HELTEC_BOARD, SLOW_CLK_TPYE);

    VextON();
    delay(200);

    display.init();
    display.clear();
    display.setFont(ArialMT_Plain_10);
    display.setTextAlignment(TEXT_ALIGN_LEFT);
    display.drawString(0, 0, "RX LoRa");
    display.drawString(0, 15, "Inicializando...");
    display.display();

    // --------------------------------------------------------
    // INICIALIZACAO DO CARTAO SD
    // --------------------------------------------------------

    sd_spi.begin(SCK, MISO, MOSI, CS);

    if (!SD.begin(CS, sd_spi) ||
        SD.cardType() == CARD_NONE)
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

    // --------------------------------------------------------
    // CALLBACKS DO RADIO
    // --------------------------------------------------------

    RadioEvents.RxDone = OnRxDone;
    RadioEvents.RxTimeout = OnRxTimeout;
    RadioEvents.RxError = OnRxError;
    RadioEvents.TxDone = OnTxDone;
    RadioEvents.TxTimeout = OnTxTimeout;

    // --------------------------------------------------------
    // INICIALIZACAO DO RADIO
    // --------------------------------------------------------

    Radio.Init(&RadioEvents);
    Radio.SetChannel(RF_FREQUENCY);

    Radio.SetRxConfig(
        MODEM_LORA, LORA_BANDWIDTH,
        LORA_SPREADING_FACTOR, LORA_CODINGRATE,
        0, LORA_PREAMBLE_LENGTH, LORA_SYMBOL_TIMEOUT,
        LORA_FIX_LENGTH_PAYLOAD_ON, 0, true, 0, 0,
        LORA_IQ_INVERSION_ON, true
    );

    Radio.SetTxConfig(
        MODEM_LORA, TX_OUTPUT_POWER, 0,
        LORA_BANDWIDTH, LORA_SPREADING_FACTOR,
        LORA_CODINGRATE, LORA_PREAMBLE_LENGTH,
        LORA_FIX_LENGTH_PAYLOAD_ON, true, 0, 0,
        LORA_IQ_INVERSION_ON, 3000
    );

    Serial.println("Radio inicializado.");
    Serial.println("Cartao SD inicializado.");
    Serial.println("Aguardando PING/START.");
    Serial.println("Nao precisa reiniciar entre experimentos.");

    display.clear();
    display.drawString(0, 0, "RX LoRa pronto");
    display.drawString(0, 15, "Aguardando TX...");
    display.display();

    Radio.Rx(0);
}

// ============================================================
// LOOP
// ============================================================

void loop()
{
    Radio.IrqProcess();

    static unsigned long ultimoDisplay = 0;

    if (millis() - ultimoDisplay >= 250)
    {
        ultimoDisplay = millis();

        if (sessaoAtiva && !experimentoFinalizado)
        {
            atualizarDisplay();
        }
    }

    if (pacoteRecebido)
    {
        pacoteRecebido = false;

        if (pacotesRecebidosTotal % 10 == 0)
        {
            bool integridadeOK =
                verificarIntegridade(ultimoPayload, tamanhoRecebido);

            Serial.print("EXP ");
            Serial.print(numeroExperimento);

            Serial.print(" | Validos: ");
            Serial.print(pacotesRecebidos);

            Serial.print(" | Recebidos: ");
            Serial.print(pacotesRecebidosTotal);

            Serial.print(" | RSSI: ");
            Serial.print(rssiAtual);
            Serial.print(" dBm");

            Serial.print(" | SNR: ");
            Serial.print(snrAtual);
            Serial.print(" dB");

            Serial.print(" | Integridade: ");
            Serial.println(integridadeOK ? "OK" : "ERRO");
        }
    }
}

