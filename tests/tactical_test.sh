#!/usr/bin/env bash
set -euo pipefail

ENGINE="./AlienPlays-C"
if [[ ! -x "$ENGINE" ]]; then
    echo "Variklis nerastas. Pirmiausia vykdyk: make"
    exit 1
fi

run_test() {
    local name="$1"
    local fen="$2"
    local expected="$3"

    local output
    output=$(
        {
            printf 'uci\n'
            printf 'isready\n'
            printf 'position fen %s\n' "$fen"
            printf 'go depth 4\n'
            sleep 2
            printf 'quit\n'
        } | "$ENGINE" 2>/dev/null
    )

    local best
    best=$(printf '%s\n' "$output" | awk '/^bestmove / {print $2}' | tail -n1)

    if [[ "$expected" == "LEGAL" ]]; then
        if [[ "$best" =~ ^[a-h][1-8][a-h][1-8][qrbn]?$ ]]; then
            printf 'PASS  %-28s bestmove=%s\n' "$name" "$best"
        else
            printf 'FAIL  %-28s netinkamas ėjimas=%s\n' "$name" "${best:-NERASTA}"
        fi
    elif [[ "$best" == "$expected" ]]; then
        printf 'PASS  %-28s bestmove=%s\n' "$name" "$best"
    else
        printf 'FAIL  %-28s tikėtasi=%s gauta=%s\n' "$name" "$expected" "${best:-NERASTA}"
    fi
}

run_test "Matas vienu ėjimu" \
    "7k/6pp/5KQ1/8/8/8/8/8 w - - 0 1" \
    "g6g7"

run_test "Pradinis ėjimas legalus" \
    "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1" \
    "LEGAL"
