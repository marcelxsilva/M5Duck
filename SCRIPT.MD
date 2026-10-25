# Como criar scripts

## Onde colocar

No cartão SD, dentro de `/scripts/<nome-da-fila>/`. Cada subpasta é uma fila, e o nome dela é o que aparece na tela do M5 (ex.: `browser`, `terminal`).

Dentro da pasta, os arquivos `.txt` rodam em ordem alfabética — numere pra garantir a ordem:

```
/scripts/browser/01_abrir.txt
/scripts/browser/02_youtube.txt
/scripts/terminal/01_cmd.txt
/scripts/terminal/02_comando.txt
```

## Formato

Um comando por linha, texto puro (.txt), sem acento no conteúdo digitado (layout de teclado é US International).

| Comando | O que faz | Exemplo |
|---|---|---|
| `STRING <texto>` | Digita o texto literal | `STRING notepad` |
| `ENTER` | Aperta Enter | `ENTER` |
| `TAB` | Aperta Tab | `TAB` |
| `DELAY <ms>` | Espera X milissegundos | `DELAY 500` |
| `GUI <letra>` | Windows + letra | `GUI R` (abre o Executar) |
| (linha vazia) | Ignorada | — |
| `# texto` | Comentário, ignorada | `# abre o executar` |

## Exemplo completo

Fila `browser` com dois scripts:

`/scripts/browser/01_abrir.txt`:
```
# abre o executar e inicia o navegador
GUI R
DELAY 500
STRING chrome
ENTER
```

`/scripts/browser/02_youtube.txt`:
```
# espera o navegador abrir e vai pro youtube
DELAY 2000
STRING youtube.com
ENTER
```

No M5, ao escolher a fila `browser` e confirmar, os dois arquivos são executados em sequência, como se fossem um só.

## Limites

- Até **~100 caracteres por linha** (buffer do Digispark). Linha maior que isso é cortada.
- Um `DELAY` numa linha só não pode passar de **~15 segundos** — depois disso o M5 entende que o Digispark parou de responder e cancela a fila.
- Sem limite de tamanho total de script ou de quantos arquivos tem numa fila: cada linha é enviada e digitada na hora, nada fica guardado no Digispark.
- `GUI <letra>` só aceita uma letra ou número depois do espaço (ex.: `GUI R`, `GUI E`). Combinações com mais de uma tecla (ex.: Ctrl+Shift+Esc) não são suportadas ainda.

## Erros comuns

| Sintoma | Causa provável |
|---|---|
| Linha não faz nada | Comando escrito errado (confere espaço depois de `STRING`/`DELAY`/`GUI`) |
| Script para no meio | Algum `DELAY` muito longo (>15s) fazendo o M5 cancelar por timeout |
| Caracteres errados ao digitar `STRING` | Texto com acento ou caractere fora do teclado US International |
| Pasta não aparece no M5 | Nome da pasta errado, ou ela está fora de `/scripts` no SD |
