# RX LoRa — Receptor e Registro de Dados

## 1. Descrição do projeto

Este firmware implementa o receptor (RX) de um sistema de comunicação LoRa utilizando a placa Heltec WiFi LoRa 32 V3.2, equipada com o microcontrolador ESP32-S3.

Sua função é receber os pacotes enviados pelo transmissor (TX), verificar a integridade dos dados, registrar informações da comunicação e apresentar os resultados do experimento.

O receptor trabalha com pacotes de dados de 255 bytes contendo uma sequência conhecida, de `0x00` a `0xFE`. Também reconhece o pacote especial de finalização, preenchido integralmente com `0xFF`.

A implementação utiliza um protocolo de controle entre TX e RX, com mensagens PING/PONG e START/ACKS, permitindo verificar a disponibilidade do receptor, iniciar sessões de experimentação e confirmar a preparação do sistema antes da transmissão dos dados.

Além do monitoramento pela porta serial e pelo display OLED, o firmware registra os dados em arquivos TXT e CSV em um cartão microSD, permitindo posterior análise em ferramentas como o Microsoft Excel.

O receptor também permite executar múltiplos experimentos sem reinicializar a placa, criando arquivos exclusivos para cada sessão e mantendo os registros anteriores.

## 2. Objetivos

- Receber os pacotes transmitidos pelo TX.
- Implementar um protocolo de controle para inicialização e confirmação das sessões.
- Verificar o tamanho e a integridade dos dados recebidos.
- Identificar o pacote especial de finalização.
- Contabilizar separadamente os pacotes íntegros, os pacotes recebidos com tamanho correto e os pacotes inválidos.
- Registrar RSSI e SNR associados aos pacotes.
- Calcular estatísticas de recepção e estimativas de perda de pacotes.
- Exibir informações durante a execução no display OLED.
- Salvar os registros do experimento no cartão microSD.
- Criar arquivos exclusivos para cada experimento, sem sobrescrever os registros anteriores.
- Registrar individualmente os bytes recebidos no arquivo CSV.
- Permitir a realização de experimentos consecutivos sem reinicialização do receptor.
- Disponibilizar os dados para análise posterior.

## 3. Hardware e software

### Hardware

- Heltec WiFi LoRa 32 V3.2.
- ESP32-S3.
- Display OLED integrado.
- Cartão microSD e módulo/interface compatível com a ligação utilizada no projeto.
- Antena compatível com a frequência configurada.

### Software

- Arduino IDE.
- Biblioteca Heltec ESP32 Dev-Boards.
- Arquivo `LoRaConfig.h`.
- Bibliotecas de suporte ao display OLED e ao cartão SD utilizadas pelo firmware.

## 4. Parâmetros de recepção

O receptor utiliza os parâmetros definidos em `LoRaConfig.h`.

| Parâmetro | Descrição |
|---|---|
| Frequência | Frequência de operação do enlace LoRa |
| Largura de banda | Bandwidth (BW) do LoRa |
| Spreading Factor (SF) | Fator de espalhamento |
| Coding Rate (CR) | Taxa de codificação |
| Potência de transmissão | Potência utilizada pelo RX nas respostas de controle |
| Tamanho esperado dos dados | 255 bytes |
| Quantidade de pacotes esperados | 2.400 pacotes normais |
| Pacote de finalização | 255 bytes preenchidos com `0xFF` |

Para que a comunicação funcione corretamente, TX e RX devem utilizar parâmetros de rádio compatíveis, incluindo frequência, largura de banda, fator de espalhamento, taxa de codificação e demais configurações pertinentes.

O RX também transmite mensagens de controle ao TX, como PONG e ACKS. Por isso, sua configuração de transmissão deve ser compatível com a configuração de recepção do transmissor.

## 5. Protocolo de comunicação e controle de sessão

### 5.1. PING/PONG

O protocolo PING/PONG permite verificar se o receptor está disponível para comunicação.

Quando recebe uma mensagem PING válida, o RX responde com uma mensagem PONG. Essa troca permite ao TX verificar a disponibilidade do receptor antes de prosseguir com a inicialização do experimento.

### 5.2. START/ACKS

A mensagem START solicita ao receptor o início de uma nova sessão de experimentação.

Ao receber a solicitação, o RX identifica a sessão por meio de um valor denominado *nonce*, utilizado para associar a solicitação à respectiva confirmação.

Após preparar a sessão e criar os arquivos de registro, o RX envia uma mensagem ACKS ao TX, contendo:

- Identificador da mensagem de confirmação.
- Nonce da sessão solicitada.
- Número do experimento criado pelo receptor.

Essa confirmação permite ao TX identificar que o RX preparou a sessão e informar o número do experimento correspondente.

Se o RX receber novamente uma solicitação START com o mesmo nonce da sessão ativa, ele reenvia a confirmação, permitindo lidar com situações em que o ACKS anterior não tenha sido recebido pelo transmissor.

### 5.3. Execução de múltiplas sessões

O RX permite iniciar novos experimentos sem reinicializar a placa.

Ao iniciar uma nova sessão, o firmware fecha os arquivos anteriores, reinicializa os contadores e as estatísticas e prepara novos arquivos para registrar os dados recebidos.

Cada sessão possui seu próprio número de experimento e seus respectivos arquivos TXT e CSV.

## 6. Recepção e validação dos pacotes

### 6.1. Recepção

Quando um pacote é recebido, o firmware obtém os dados disponibilizados pelo rádio e registra informações associadas à recepção, incluindo tamanho, RSSI e SNR.

O processamento distingue as mensagens de controle, os pacotes normais de dados e o pacote especial de finalização.

As mensagens de controle são tratadas separadamente da contagem dos pacotes normais do experimento.

### 6.2. Verificação de integridade

Os pacotes normais esperados contêm a sequência:

`00 01 02 03 04 05 06 07 ... FC FD FE`

A verificação de integridade compara cada byte recebido com o valor esperado para sua posição.

Um pacote é considerado íntegro quando possui exatamente 255 bytes e todos os bytes correspondem à sequência prevista.

Essa verificação permite identificar alterações no conteúdo recebido, além de possibilitar a separação entre pacotes íntegros e pacotes com erro de integridade.

### 6.3. Pacote de finalização

O TX envia um pacote de 255 bytes preenchido integralmente com `0xFF` após concluir os envios normais.

O RX reconhece esse padrão para identificar o término da sequência de transmissão.

O pacote de finalização é tratado separadamente dos pacotes normais, não sendo contabilizado como um pacote de dados do experimento.

Ao receber a finalização durante uma sessão ativa, o RX encerra os arquivos, apresenta as estatísticas finais e permanece em recepção, aguardando uma nova sessão.

### 6.4. Pacotes com tamanho incorreto

Pacotes recebidos com tamanho diferente de 255 bytes, quando não reconhecidos como mensagens de controle válidas, são contabilizados como inválidos.

Esses pacotes são informados pela porta serial, mas não são registrados como pacotes normais no arquivo TXT/CSV pela rotina de salvamento de dados.

## 7. Métricas de comunicação

### 7.1. Pacotes íntegros recebidos

Representa a quantidade de pacotes normais de 255 bytes que passaram pela verificação de integridade.

Essa contagem é utilizada como referência para calcular a perda estimada e as médias de RSSI e SNR.

### 7.2. Total de pacotes de dados recebidos

Representa a quantidade de pacotes de dados com tamanho correto recebidos durante o experimento, incluindo aqueles cujo conteúdo apresentou erro de integridade.

Essa métrica permite diferenciar a recepção física de um pacote da validação de seu conteúdo.

### 7.3. Pacotes inválidos

Contabiliza pacotes de dados com tamanho incorreto ou com erro de integridade.

Os pacotes com tamanho correto, mas com conteúdo inválido, também são contabilizados separadamente nas estatísticas de integridade.

### 7.4. RSSI

O *Received Signal Strength Indicator* representa uma estimativa da potência do sinal recebido, expressa em dBm.

Em condições comparáveis, valores menos negativos geralmente indicam um sinal recebido mais forte.

O firmware registra o RSSI associado a cada pacote de dados de tamanho correto e calcula a média considerando os pacotes íntegros.

### 7.5. SNR

A *Signal-to-Noise Ratio* representa a relação entre o sinal e o ruído, expressa em dB.

Essa métrica ajuda a caracterizar as condições do enlace durante a recepção.

O firmware registra o SNR associado a cada pacote de dados de tamanho correto e calcula a média considerando os pacotes íntegros.

### 7.6. Percentual de perda de pacotes

O percentual de perda estimado pelo RX é calculado com base na quantidade de pacotes esperados e na quantidade de pacotes íntegros recebidos:

\[
P_p = \frac{N_t-N_r}{N_t}\times100
\]

Em que:

- \(P_p\): percentual de perda estimado.
- \(N_t\): quantidade esperada de pacotes normais transmitidos.
- \(N_r\): quantidade de pacotes normais íntegros recebidos.

Neste experimento, o TX está configurado para enviar 2.400 pacotes normais.

Como o RX utiliza a quantidade de pacotes íntegros na estimativa, um pacote recebido com conteúdo corrompido contribui para a diferença em relação ao total esperado. Portanto, essa métrica não representa necessariamente apenas pacotes que deixaram de ser recebidos pelo rádio.

A contagem de pacotes íntegros também não identifica individualmente cada pacote transmitido. Assim, o RX não consegue distinguir com certeza pacotes perdidos, duplicados ou recebidos fora da sequência esperada.

## 8. Display OLED

O display OLED apresenta informações para acompanhamento durante o experimento.

Durante a recepção, a interface exibe:

- Número do experimento atual.
- Quantidade de pacotes íntegros recebidos em relação aos 2.400 esperados.
- Percentual estimado de perda.
- RSSI atual.
- SNR atual.

Ao término do experimento, o display apresenta um resumo contendo a quantidade de pacotes íntegros, a quantidade estimada de pacotes perdidos, o percentual estimado de perda e o RSSI médio.

A atualização periódica permite acompanhar o experimento sem depender exclusivamente do monitor serial.

## 9. Registro dos dados no cartão microSD

O firmware armazena os dados dos pacotes recebidos em dois formatos: TXT e CSV.

A criação dos arquivos ocorre quando o RX recebe uma solicitação válida de início de sessão e consegue preparar o experimento.

### 9.1. Identificação dos arquivos

Os arquivos são identificados pelo número do experimento, seguindo o padrão:

- `lora_experimento_001.txt`
- `lora_experimento_001.csv`

Para experimentos seguintes, o firmware procura o próximo número disponível, preservando os registros anteriores.

### 9.2. Arquivo TXT

O arquivo TXT apresenta os registros em formato textual, facilitando a leitura direta dos resultados.

Cada registro contém:

| Campo | Descrição |
|---|---|
| `TEMPO_MS` | Tempo decorrido desde o início do experimento, em milissegundos |
| `DADOS_HEX` | Conteúdo completo do pacote em hexadecimal |
| `TAMANHO` | Quantidade de bytes recebidos |
| `RSSI` | RSSI associado ao pacote |
| `SNR` | SNR associado ao pacote |
| `INTEGRIDADE` | Resultado da verificação: `OK` ou `ERRO` |

### 9.3. Arquivo CSV

O arquivo CSV organiza os dados em colunas, permitindo importação no Microsoft Excel e em outras ferramentas de análise.

Os campos gerais incluem:

| Campo | Descrição |
|---|---|
| `experimento` | Número do experimento |
| `tempo_ms` | Tempo decorrido desde o início da sessão |
| `tamanho` | Quantidade de bytes recebidos |
| `rssi` | RSSI associado ao pacote |
| `snr` | SNR associado ao pacote |
| `integridade` | Resultado da verificação dos dados |
| `byte_000` a `byte_254` | Valores individuais dos bytes recebidos, em decimal |

Cada byte é armazenado em sua própria coluna, permitindo examinar e comparar o conteúdo recebido em cada posição do pacote.

O arquivo utiliza ponto e vírgula (`;`) como separador, formato conveniente para configurações regionais do Excel que utilizam vírgula como separador decimal.

### 9.4. Gravação e fechamento dos arquivos

Os arquivos são sincronizados com o cartão microSD a cada dez pacotes de dados recebidos, reduzindo a necessidade de gravações frequentes.

Ao receber o pacote de finalização, o firmware sincroniza e fecha os arquivos antes de apresentar os resultados finais.

Quando uma nova sessão é iniciada, os arquivos da sessão anterior também são fechados antes da criação dos novos registros.

## 10. Resultado final

Ao término da recepção, o firmware apresenta as estatísticas calculadas durante o experimento pela porta serial e pelo display OLED.

Os resultados incluem:

- Número do experimento.
- Quantidade esperada de pacotes normais.
- Quantidade de pacotes íntegros recebidos.
- Total de pacotes de dados recebidos com tamanho correto.
- Quantidade estimada de pacotes perdidos.
- Percentual estimado de perda.
- Quantidade de pacotes inválidos.
- Quantidade de pacotes com integridade OK.
- Quantidade de pacotes com erro de integridade.
- RSSI médio dos pacotes íntegros.
- SNR médio dos pacotes íntegros.

Os resultados devem ser interpretados de acordo com os critérios de contagem adotados e comparados com os dados registrados pelo TX.

A perda apresentada pelo RX é uma estimativa baseada na diferença entre o total esperado e os pacotes íntegros recebidos. Para uma análise mais precisa de perdas, duplicações e ordem de recepção, seria necessário identificar individualmente cada pacote transmitido.

## 11. Como executar

1. Instale a Arduino IDE e o suporte à placa Heltec ESP32.
2. Instale as bibliotecas necessárias para o rádio, o display e o cartão microSD.
3. Configure `LoRaConfig.h` com parâmetros compatíveis com o transmissor.
4. Abra o projeto RX no ambiente de desenvolvimento.
5. Selecione a placa Heltec WiFi LoRa 32 V3.2 e a porta serial correta.
6. Insira o cartão microSD preparado para o sistema de arquivos utilizado pelo código.
7. Grave o firmware no receptor.
8. Ligue o RX e aguarde a inicialização do rádio e do cartão microSD.
9. Inicie o TX e aguarde o procedimento de verificação e inicialização da sessão.
10. Acompanhe os dados pelo display OLED e pelo monitor serial.
11. Após o recebimento do pacote de finalização, consulte os resultados e os arquivos gravados no cartão microSD.
12. Para realizar outro experimento, inicie uma nova sessão pelo TX, sem precisar reiniciar o RX.

**Importante:** o receptor depende do cartão microSD para registrar os experimentos. Caso a inicialização do cartão falhe, o firmware informa o erro e permanece aguardando a correção do problema.

## 12. Integração com o TX

O receptor foi projetado para funcionar em conjunto com o firmware TX deste projeto.

O TX utiliza um protocolo de controle para verificar a disponibilidade do RX, solicitar o início da sessão e receber a confirmação antes de transmitir os pacotes normais.

Durante o experimento, o TX envia 2.400 pacotes normais de 255 bytes e, ao final, um pacote adicional preenchido com `0xFF`.

O RX recebe e valida os pacotes, registra as métricas de comunicação, armazena os dados no cartão microSD e identifica a finalização.

A troca de mensagens PING/PONG e START/ACKS organiza a inicialização e permite relacionar os dados registrados ao número de experimento atribuído pelo receptor.

A comparação entre os dados enviados pelo TX e os registros do RX permite avaliar a qualidade do enlace LoRa, considerando a recepção dos pacotes, a integridade dos dados e as condições do sinal durante a transmissão.

## 13. Novas funcionalidades em relação à versão anterior

As principais adições implementadas nesta versão do RX são:

- Protocolo PING/PONG para verificação da disponibilidade do receptor.
- Protocolo START/ACKS para inicialização e confirmação das sessões.
- Identificação das sessões por nonce.
- Comunicação do número do experimento ao TX.
- Execução de experimentos consecutivos sem reinicialização da placa.
- Criação automática de arquivos TXT e CSV exclusivos para cada experimento.
- Proteção dos registros anteriores contra sobrescrita por sessões posteriores.
- Registro individual dos bytes recebidos nas colunas `byte_000` a `byte_254` do CSV.
- Separação entre total de pacotes de dados recebidos, pacotes íntegros e pacotes inválidos.
- Contagem específica dos resultados de integridade.
- Cálculo das médias de RSSI e SNR com base nos pacotes íntegros.
- Atualização do display OLED com o progresso e os resultados finais.
- Fechamento dos arquivos e apresentação das estatísticas ao final de cada sessão.
