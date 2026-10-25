# DigiStack

Ferramenta pra automatizar a instalação/execução de um programa em várias máquinas: o M5StickC Plus2 lê uma fila de scripts do SD e **tunela** os comandos pro Digispark em tempo real; o Digispark digita cada comando via USB (HID teclado) assim que chega, sem guardar nada.

## Como funciona

M5 e Digispark ficam ligados o tempo todo (um fio só, nos dois sentidos). O Digispark precisa estar plugado ao mesmo tempo no M5 (recebendo comandos) **e** na porta USB do computador alvo (digitando). Não existe uma etapa separada de "gravar e depois desplugar" — é tudo em tempo real.

1. M5StickC Plus2 lê os arquivos `.txt` da pasta `/scripts` do SD, em ordem alfabética (isso define a fila: `01_abrir.txt`, `02_instalar.txt`, `03_fechar.txt`...).
2. Botão B no M5 começa a enviar a fila: linha por linha, de todos os scripts, em sequência.
3. Cada linha vai pro Digispark pelo fio de dados; o Digispark digita aquela linha na hora (via USB, no computador em que está plugado) e manda de volta um sinal de "ok, pode mandar a próxima" pelo mesmo fio.
4. M5 só manda a linha seguinte depois desse "ok" — por isso não existe mais limite de 512 bytes por script: nada fica guardado no Digispark, só passa.

## Conexão entre as placas

Fio único, bidirecional (half-duplex): M5 manda comando, Digispark responde "ok" pelo mesmo fio.

| Sinal | M5StickC Plus2 | Digispark | Obs. |
|---|---|---|---|
| Dados (os dois lados usam o mesmo fio) | G25 | P2 | Precisa de resistor de pull-up (~10kΩ) nesse fio até o 3.3V, pra linha ficar em nível alto quando ninguém está transmitindo. |
| Alimentação | 3.3V | VCC/5V | ATtiny85 aceita 3.3V direto. Confirme com multímetro qual pino do conector de baixo do M5 é 3.3V antes de ligar. |
| Terra | GND | GND | Obrigatório, senão o sinal de dados não funciona. |

Importante: essa alimentação do M5 é só para o Digispark funcionar enquanto recebe/digita. A energia pra fazer o USB-HID conversar com o computador alvo vem da própria porta USB do computador, não do M5.

SD do M5 (módulo genérico, fios soltos):

| Sinal | M5StickC Plus2 |
|---|---|
| CS | G26 |
| SCK | G0 |
| MOSI | G32 |
| MISO | G33 |

O M5StickC Plus2 não tem slot de SD embutido — precisa de um módulo externo ligado nesses 4 pinos + 3.3V + GND.

## Scripts

Ficam em `/scripts/*.txt` no cartão SD. Linguagem simples, uma instrução por linha:

- `STRING <texto>` — digita o texto literal
- `DELAY <ms>` — espera
- `ENTER`, `TAB` — teclas
- `GUI <letra>` — Windows + letra (ex.: `GUI R` abre o Executar)
- linha vazia ou começando com `#` — ignorada

Limite: **~100 caracteres por linha** (buffer do Digispark). Não tem mais limite de tamanho total do script nem da fila — é tudo tunelado, linha por linha, sem guardar em memória. Um `DELAY` numa linha só não pode passar de ~15s, senão o M5 acha que o Digispark parou de responder e cancela a fila.

## Onde está o quê

| O quê | Onde |
|---|---|
| Firmware do M5StickC Plus2 | `platformio.ini` + `src/main.cpp` (raiz do projeto) |
| Firmware do Digispark | `digispark/platformio.ini` + `digispark/src/main.cpp` |
| Compilar/gravar M5 | `pio run` / `pio run -t upload` na raiz |
| Compilar/gravar Digispark | `pio run` / `pio run -t upload` dentro de `digispark/` |

## Limites conhecidos

- Layout de teclado fixo em US International (vem da lib do Digispark).
- Como tudo é tunelado em tempo real, o Digispark **precisa continuar ligado ao M5** (fio de dados + alimentação) durante todo o tempo em que está plugado no computador alvo. Não dá pra desplugar do M5 e levar só o Digispark.
- Se faltar o resistor de pull-up no fio de dados, a linha pode ficar em nível indefinido quando ninguém está transmitindo e gerar leitura de lixo nos dois lados.
