# RX LoRa — Receptor e Registro de Dados

## 1. Descrição do projeto

Este firmware implementa o receptor (RX) de um sistema de comunicação LoRa utilizando a placa Heltec WiFi LoRa 32 V3.2, equipada com o microcontrolador ESP32-S3

Sua função é receber os pacotes enviados pelo transmissor (TX), verificar o conteúdo recebido, registrar informações da comunicação e apresentar os resultados do experimento.

O receptor foi desenvolvido para trabalhar com pacotes de 255 bytes contendo uma sequência conhecida, de `0x00` a `0xFE`. Também reconhece o pacote especial de finalização, preenchido com `0xFF`.

Além do monitoramento pela porta serial e pelo display OLED, a implementação do experimento contempla o registro dos dados em arquivos TXT e CSV em um cartão microSD, permitindo posterior análise em ferramentas como o Microsoft Excel.

## 2. Objetivos

* Receber os pacotes transmitidos pelo TX.
* Verificar o tamanho e a integridade dos dados.
* Identificar o pacote de finalização.
* Contabilizar os pacotes recebidos e inválidos.
* Registrar RSSI e SNR.
* Calcular estatísticas de recepção e perda de pacotes.
* Exibir informações durante a execução no display OLED.
* Salvar os registros do experimento no cartão microSD.
* Disponibilizar os dados para análise posterior.

## 3. Hardware e software

**Hardware**

* Heltec WiFi LoRa 32 V3.2.
* ESP32-S3.
* Display OLED integrado.
* Cartão microSD e módulo/interface compatível com a ligação utilizada no projeto.
* Antena compatível com a frequência configurada.

**Software**

* Arduino IDE.
* Biblioteca Heltec ESP32 Dev-Boards.
* Arquivo `LoRaConfig.h`.
* Bibliotecas de suporte ao display OLED e ao cartão SD utilizadas pelo firmware.

## 4. Parâmetros de recepção

O receptor utiliza os parâmetros definidos em `LoRaConfig.h`.

| Parâmetro             | Descrição                        |
| --------------------- | -------------------------------- |
| Frequência            | Frequência de operação do enlace |
| Largura de banda      | Bandwidth (BW) do LoRa           |
| Spreading Factor (SF) | Fator de espalhamento            |
| Coding Rate (CR)      | Taxa de codificação              |
| Tamanho esperado      | 255 bytes                        |
| Pacote de finalização | 255 bytes preenchidos com `0xFF` |

Para que a comunicação funcione corretamente, TX e RX devem utilizar parâmetros de rádio compatíveis.

## 5. Recepção e validação dos pacotes

### 5.1. Recepção

Quando um pacote é recebido, o firmware obtém os dados disponibilizados pelo rádio e registra informações associadas à recepção, incluindo tamanho, RSSI e SNR.

O processamento permite distinguir os pacotes normais do pacote especial de finalização.

### 5.2. Verificação de integridade

Os pacotes normais esperados contêm a sequência:

```text
00 01 02 03 04 05 06 07 ... FD FE
```

A verificação de integridade compara os bytes recebidos com os valores esperados para cada posição.

Um pacote pode ser considerado íntegro quando possui o tamanho esperado e todos os bytes correspondem à sequência prevista.

Essa verificação permite identificar alterações no conteúdo recebido, além de pacotes com tamanho incorreto.

### 5.3. Pacote de finalização

O TX envia um pacote de 255 bytes preenchido integralmente com `0xFF` após concluir os envios normais.

O RX reconhece esse padrão para identificar o término da sequência.

Esse pacote deve ser tratado separadamente dos pacotes normais, para não distorcer a contagem de dados do experimento.

## 6. Métricas de comunicação

### 6.1. Pacotes recebidos

Representa a quantidade de pacotes normais identificados pelo receptor conforme os critérios implementados no firmware.

### 6.2. RSSI

O *Received Signal Strength Indicator* representa uma estimativa da potência do sinal recebido, expressa em dBm.

Em condições comparáveis, valores menos negativos geralmente indicam um sinal recebido mais forte.

### 6.3. SNR

A *Signal-to-Noise Ratio* representa a relação entre o sinal e o ruído, expressa em dB.

Essa métrica ajuda a caracterizar as condições do enlace durante a recepção.

### 6.4. Percentual de perda de pacotes

Quando o total efetivamente transmitido é conhecido, a perda pode ser calculada por:

$$
P_p = \frac{N_t-N_r}{N_t}\times100
$$

Em que:

* \(P_p\): percentual de perda de pacotes.
* \(N_t\): quantidade de pacotes normais transmitidos.
* \(N_r\): quantidade de pacotes normais recebidos segundo o critério adotado.

Para este experimento, o TX está configurado para enviar 2.400 pacotes normais.

A contagem do receptor deve excluir o pacote especial de finalização. Pacotes duplicados ou inválidos também precisam ser tratados conforme o critério definido para a análise.

## 7. Display OLED

O display OLED apresenta informações de acompanhamento durante o experimento.

A interface pode exibir dados como:

* Identificação do experimento.
* Quantidade de pacotes recebidos.
* Percentual de perda estimado.
* RSSI atual.
* Informações do resultado final.

O display permite acompanhar a recepção sem depender exclusivamente do monitor serial.

## 8. Registro dos dados no cartão microSD

O firmware contempla o armazenamento dos dados recebidos em dois formatos: TXT e CSV.

### 8.1. Arquivo TXT

O arquivo TXT apresenta os registros em formato textual, facilitando a leitura direta dos resultados.

### 8.2. Arquivo CSV

O arquivo CSV organiza os dados em colunas, permitindo importação no Microsoft Excel e em outras ferramentas de análise.

Os campos previstos para os registros são:

| Campo             | Descrição                                                       |
| ----------------- | --------------------------------------------------------------- |
| `pacote`          | Número sequencial do registro                                   |
| `tempo_ms`        | Tempo decorrido desde o início do experimento, em milissegundos |
| `dados_recebidos` | Bytes recebidos, representados em hexadecimal                   |
| `tamanho`         | Quantidade de bytes recebidos                                   |
| `rssi`            | RSSI associado ao pacote                                        |
| `snr`             | SNR associado ao pacote                                         |
| `integridade`     | Resultado da verificação dos dados                              |

O arquivo CSV utiliza separador de ponto e vírgula (`;`), formato conveniente para configurações regionais do Excel que utilizam vírgula como separador decimal.

A disponibilidade efetiva de cada campo depende da implementação utilizada no firmware RX.

## 9. Resultado final

Ao término da recepção, o firmware apresenta as estatísticas calculadas durante o experimento, conforme as variáveis implementadas no código.

Os resultados podem incluir:

* Total de pacotes recebidos.
* Total de pacotes inválidos.
* Percentual de perda calculado.
* RSSI médio.
* SNR médio.
* Quantidade de pacotes íntegros.

Os valores devem ser interpretados de acordo com os critérios de contagem adotados e comparados com os dados registrados pelo TX.

## 10. Como executar

1. Instale a Arduino IDE e o suporte à placa Heltec ESP32.
2. Instale as bibliotecas necessárias para o rádio, o display e o cartão microSD.
3. Configure `LoRaConfig.h` com parâmetros compatíveis com o transmissor.
4. Abra o projeto RX no ambiente de desenvolvimento.
5. Selecione a placa Heltec WiFi LoRa 32 V3.2 e a porta serial correta.
6. Grave o firmware no receptor.
7. Insira o cartão microSD preparado para o sistema de arquivos utilizado pelo código.
8. Ligue o receptor antes de iniciar a transmissão.
9. Acompanhe os dados no OLED e na porta serial.
10. Após o experimento, abra os arquivos gravados no cartão microSD para analisar os resultados.

## 11. Integração com o TX

O receptor foi projetado para funcionar em conjunto com o firmware TX deste repositório.

O TX transmite 2.400 pacotes normais de 255 bytes e, ao final, um pacote adicional preenchido com `0xFF`.

O RX recebe e valida os pacotes, registra as métricas de comunicação e identifica a finalização. A comparação entre os pacotes enviados e os efetivamente recebidos permite avaliar a qualidade do enlace LoRa.

