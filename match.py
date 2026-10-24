import time
from pathlib import Path

import chess
import chess.pgn
import chess.engine

ALIEN = "./AlienPlays-C"
STOCKFISH = "/usr/bin/stockfish"


def main():
    games_count = 100
    move_time = 0.5 # seconds per move

    output_dir = Path("matches")
    output_dir.mkdir(exist_ok=True)
    pgn_path = output_dir / "alienplays_vs_stockfish.pgn"
    summary_path = output_dir / "summary.txt"

    results = {"win": 0, "draw": 0, "loss": 0}
    started = time.time()
    alien_score = 0.0

    print("AlienPlays-C vs Stockfish 18")
    print(f"Games: {games_count} | Time per move: {move_time}s")

    with (
        chess.engine.SimpleEngine.popen_uci(ALIEN) as alien,
        chess.engine.SimpleEngine.popen_uci(STOCKFISH) as stockfish,
        pgn_path.open("w", encoding="utf-8") as pgn_file,
    ):
        print("Alien:", alien.id.get("name"))
        print("Stockfish:", stockfish.id.get("name"))

        limit = chess.engine.Limit(time=move_time)

        for game_no in range(1, games_count + 1):
            alien_white = game_no % 2 == 1
            white = alien if alien_white else stockfish
            black = stockfish if alien_white else alien

            board = chess.Board()
            game = chess.pgn.Game()
            game.headers["Event"] = "AlienPlays-C vs Stockfish 18"
            game.headers["Round"] = str(game_no)
            game.headers["White"] = white.id.get("name", "White")
            game.headers["Black"] = black.id.get("name", "Black")
            node = game

            print(
                f"\nGame {game_no}/{games_count}: "
                f"AlienPlays {'White' if alien_white else 'Black'}"
            )

            while not board.is_game_over(claim_draw=True):
                engine = white if board.turn == chess.WHITE else black
                response = engine.play(board, limit)

                if response.move is None or response.move not in board.legal_moves:
                    raise RuntimeError(
                        f"Invalid move from {engine.id}: {response.move}\n"
                        f"FEN: {board.fen()}"
                    )

                node = node.add_variation(response.move)
                board.push(response.move)

            outcome = board.outcome(claim_draw=True)
            result = outcome.result() if outcome else "*"
            game.headers["Result"] = result
            game.headers["Termination"] = (
                outcome.termination.name if outcome else "unknown"
            )

            if result == "1/2-1/2":
                results["draw"] += 1
                alien_score += 0.5
            elif (result == "1-0" and alien_white) or (
                result == "0-1" and not alien_white
            ):
                results["win"] += 1
                alien_score += 1.0
            else:
                results["loss"] += 1

            pgn_file.write(str(game) + "\n\n")
            pgn_file.flush()

            print(
                f"Result: {result} | "
                f"AlienPlays score: {alien_score:.1f}/{game_no}"
            )

    elapsed = time.time() - started
    report = (
        f"Games: {games_count}\n"
        f"AlienPlays wins: {results['win']}\n"
        f"Draws: {results['draw']}\n"
        f"AlienPlays losses: {results['loss']}\n"
        f"Score: {alien_score:.1f}/{games_count} "
        f"({alien_score / games_count * 100:.1f}%)\n"
        f"Elapsed: {elapsed:.1f} seconds\n"
        f"PGN: {pgn_path}\n"
    )

    summary_path.write_text(report, encoding="utf-8")
    print("\n=== FINAL RESULTS ===")
    print(report)


if __name__ == "__main__":
    main()