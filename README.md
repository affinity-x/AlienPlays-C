# AlienPlays-C

AlienPlays-C is an experimental chess engine written in C, focused on chess search, position evaluation, and engine development.

## Features

- UCI protocol support
- Legal move generation
- Alpha-beta search
- Position evaluation
- Transposition table
- Neural-network evaluation experiments
- Opening-book support
- PGN and training utilities

## Requirements

- GCC
- GNU Make

## Build

Clone the repository and compile the engine:

```bash
git clone https://github.com/affinity-x/AlienPlays-C.git
cd AlienPlays-C
make
```

## Run

Start the engine:

```bash
./AlienPlays-C
```

The engine is designed to communicate with compatible chess GUIs through the Universal Chess Interface (UCI) protocol.

## Runtime Files

Some features expect external data files:

- `cerebrum-v2.0.nn` — neural-network evaluation data
- `codekiddy.bin` — opening-book data

These files are not included in this repository. Some features may be unavailable without them.

## Project Status

AlienPlays-C is an experimental project under active development. Playing strength, evaluation, and compatibility may change between versions.

Bug reports, testing, and suggestions are welcome.

## License

No project-wide license has been selected yet. Until a license is added, the code remains subject to applicable copyright law. Third-party components may have separate licenses.
