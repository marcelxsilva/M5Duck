# Writing scripts

## Where they go

One script per file, in `scripts/<name>.txt`. The file name (without `.txt`) is what shows up on the M5 screen.

```
scripts/browser.txt
scripts/terminal.txt
```

After editing anything in `scripts/`, flash it again with `pio run -t uploadfs`.

## Format

One command per line, plain text, no accents in the typed content (the keyboard layout is US International).

| Command | What it does | Example |
|---|---|---|
| `STRING <text>` | Types the text literally | `STRING notepad` |
| `ENTER` | Presses Enter | `ENTER` |
| `TAB` | Presses Tab | `TAB` |
| `DELAY <ms>` | Waits X milliseconds | `DELAY 500` |
| `GUI <key>` | Windows/Command + a letter or number | `GUI R` (opens Run) |
| empty line | Ignored | |
| `# text` | Comment, ignored | `# opens Run` |

## Example

`scripts/browser.txt`:

```
# open Run and start the browser
GUI R
DELAY 500
STRING chrome
ENTER

# wait for the browser, then open a site
DELAY 2000
STRING example.com
ENTER
```

Pick `browser` on the M5, press the front button, and the file runs line by line.

## Limits

| Limit | Value |
|---|---|
| Scripts on the M5 | 30 |
| Size of each script | 16 KB |
| Characters per line | ~100 (longer lines are cut by the Digispark) |
| A single `DELAY` | ~15 s (past that the M5 assumes the Digispark stopped answering and cancels) |
| `GUI` | One letter or number only; combos like Ctrl+Shift+Esc are not supported |

The Digispark stores nothing, so there is no script length limit on its side.

## Common problems

| Symptom | Likely cause |
|---|---|
| A line does nothing | Command misspelled, or missing the space after `STRING`/`DELAY`/`GUI` |
| Script stops halfway | A `DELAY` longer than ~15 s made the M5 time out |
| Wrong characters typed | Accents or characters outside US International |
| Script missing on the M5 | File is not `.txt`, is outside `scripts/`, or `pio run -t uploadfs` was not run |
