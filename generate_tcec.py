import argparse
import chess
import chess.pgn
import chess.engine
import shutil
import math

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--input", default="TCEC-everything.pgn")
    parser.add_argument("--output", default="tcec-training.txt")
    parser.add_argument("--max-games", type=int, default=200)
    parser.add_argument("--max-positions", type=int, default=5000)
    parser.add_argument("--every", type=int, default=6,
                        help="Atrinkti poziciją kas N pusių ėjimų")
    parser.add_argument("--depth", type=int, default=8)
    args = parser.parse_args()

    engine_path = shutil.which("stockfish")
    if not engine_path:
        raise SystemExit("Klaida: nerastas stockfish vykdomasis failas.")

    engine = chess.engine.SimpleEngine.popen_uci(engine_path)
    engine.configure({"Threads": 2, "Hash": 256})

    games = 0
    positions = 0
    errors = 0

    try:
        with open(args.input, encoding="utf-8", errors="replace") as pgn, \
             open(args.output, "w", encoding="utf-8") as out:

            while games < args.max_games and positions < args.max_positions:
                game = chess.pgn.read_game(pgn)
                if game is None:
                    break

                games += 1
                if game.errors:
                    errors += len(game.errors)
                    continue

                board = game.board()

                for ply, move in enumerate(game.mainline_moves(), start=1):
                    if board.is_game_over():
                        break

                    board.push(move)

                    if ply % args.every != 0:
                        continue

                    if board.is_game_over() or not board.is_valid():
                        continue

                    try:
                        info = engine.analyse(
                            board,
                            chess.engine.Limit(depth=args.depth)
                        )
                        score = info["score"].pov(board.turn)
                        cp = score.score(mate_score=1000)

                        if cp is None:
                            continue

                        # Cerebrum mokomas pėstininkų vienetais.
                        # Vertinimas iš ėjimą darančios pusės perspektyvos.
                        evaluation = max(-10.0, min(10.0, cp / 100.0))

                        out.write(f"{board.fen()},{evaluation:.4f}\n")
                        positions += 1

                    except (chess.engine.EngineError, ValueError):
                        errors += 1
                        continue

                    if positions % 100 == 0:
                        out.flush()
                        print(
                            f"\rPartijos: {games}/{args.max_games} | "
                            f"Pozicijos: {positions}/{args.max_positions}",
                            end="", flush=True
                        )

                    if positions >= args.max_positions:
                        break

                if games % 25 == 0:
                    print(
                        f"\nPartijos: {games}, pozicijos: {positions}"
                    )

    finally:
        engine.quit()

    print(f"\nBaigta. Partijos: {games}")
    print(f"Pozicijos: {positions}")
    print(f"Klaidos: {errors}")
    print(f"Failas: {args.output}")

if __name__ == "__main__":
    main()
