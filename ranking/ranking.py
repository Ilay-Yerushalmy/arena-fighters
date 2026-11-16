"""Arena Fighters - ranking service.

Reads every match that has not been rated yet, applies an Elo rating update
for both players (weighted by how dominant the win was), stores the rating
history and recomputes the global rank of every player.

The C++ game launches this script after each match:

    python ranking/ranking.py --db data/arena.db

It can also be run by hand:

    python ranking/ranking.py --db data/arena.db --leaderboard
"""

from __future__ import annotations

import argparse
import json
import sqlite3
import sys
from dataclasses import dataclass

DEFAULT_RATING = 1000.0
BASE_K_FACTOR = 32.0
MAX_DOMINANCE_BONUS = 0.5  # a flawless victory moves ratings up to 1.5x more


@dataclass(frozen=True)
class RatingChange:
    delta_p1: float
    delta_p2: float


def expected_score(rating_a: float, rating_b: float) -> float:
    """Probability that A beats B according to the Elo model."""
    return 1.0 / (1.0 + 10.0 ** ((rating_b - rating_a) / 400.0))


def performance_multiplier(winner_hp_pct: float | None) -> float:
    """Scales the K-factor by how much health the winner had left (0-100).

    A draw (None) uses the plain K-factor.
    """
    if winner_hp_pct is None:
        return 1.0
    dominance = max(0.0, min(100.0, winner_hp_pct)) / 100.0
    return 1.0 + MAX_DOMINANCE_BONUS * dominance


def compute_rating_change(
    rating_p1: float,
    rating_p2: float,
    score_p1: float,
    winner_hp_pct: float | None,
) -> RatingChange:
    """Zero-sum Elo update. score_p1 is 1 (win), 0.5 (draw) or 0 (loss)."""
    k = BASE_K_FACTOR * performance_multiplier(winner_hp_pct)
    delta = k * (score_p1 - expected_score(rating_p1, rating_p2))
    return RatingChange(delta_p1=delta, delta_p2=-delta)


def _rating_of(conn: sqlite3.Connection, player_id: int) -> float:
    row = conn.execute("SELECT rating FROM player_stats WHERE player_id = ?", (player_id,)).fetchone()
    return float(row[0]) if row else DEFAULT_RATING


def process_unrated_matches(conn: sqlite3.Connection) -> int:
    """Rates all pending matches in chronological order. Returns how many were processed."""
    matches = conn.execute(
        """
        SELECT id, player1_id, player2_id, winner_id, p1_hp_pct, p2_hp_pct
          FROM matches
         WHERE rated = 0
         ORDER BY id
        """
    ).fetchall()

    for match_id, p1, p2, winner, p1_hp, p2_hp in matches:
        r1, r2 = _rating_of(conn, p1), _rating_of(conn, p2)

        if winner is None:
            score_p1, winner_hp = 0.5, None
        elif winner == p1:
            score_p1, winner_hp = 1.0, p1_hp
        else:
            score_p1, winner_hp = 0.0, p2_hp

        change = compute_rating_change(r1, r2, score_p1, winner_hp)

        for player_id, before, delta in ((p1, r1, change.delta_p1), (p2, r2, change.delta_p2)):
            after = before + delta
            conn.execute(
                "UPDATE player_stats SET rating = ?, updated_at = datetime('now') WHERE player_id = ?",
                (after, player_id),
            )
            conn.execute(
                "INSERT INTO rating_history (player_id, match_id, rating_before, rating_after) VALUES (?, ?, ?, ?)",
                (player_id, match_id, before, after),
            )

        conn.execute(
            "UPDATE matches SET rated = 1, p1_rating_delta = ?, p2_rating_delta = ? WHERE id = ?",
            (change.delta_p1, change.delta_p2, match_id),
        )

    return len(matches)


def recompute_ranks(conn: sqlite3.Connection) -> None:
    """Competition ranking (1, 2, 2, 4) by rating. Players without matches stay unranked."""
    conn.execute(
        """
        UPDATE player_stats
           SET rank = (
               SELECT ranked.position
                 FROM (SELECT player_id,
                              RANK() OVER (ORDER BY ROUND(rating, 2) DESC) AS position
                         FROM player_stats
                        WHERE matches_played > 0) AS ranked
                WHERE ranked.player_id = player_stats.player_id
           )
        """
    )


def update_rankings(conn: sqlite3.Connection) -> int:
    """Runs the full update in a single transaction (all-or-nothing)."""
    with conn:
        processed = process_unrated_matches(conn)
        recompute_ranks(conn)
    return processed


def print_leaderboard(conn: sqlite3.Connection, limit: int = 10) -> None:
    rows = conn.execute(
        "SELECT rank, name, rating, wins, losses, draws, win_rate FROM leaderboard "
        "WHERE matches_played > 0 LIMIT ?",
        (limit,),
    ).fetchall()
    print(f"{'#':>3}  {'Player':<16} {'Rating':>7} {'W':>4} {'L':>4} {'D':>4} {'Win%':>6}")
    for rank, name, rating, wins, losses, draws, win_rate in rows:
        print(f"{rank or '-':>3}  {name:<16} {rating:>7.0f} {wins:>4} {losses:>4} {draws:>4} {win_rate:>6.1f}")


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description="Update Arena Fighters Elo ratings and ranks.")
    parser.add_argument("--db", required=True, help="Path to the SQLite database file")
    parser.add_argument("--leaderboard", action="store_true", help="Print the leaderboard after updating")
    args = parser.parse_args(argv)

    try:
        conn = sqlite3.connect(args.db, timeout=5)
        conn.execute("PRAGMA foreign_keys = ON")
        processed = update_rankings(conn)
        print(json.dumps({"status": "ok", "matches_rated": processed}))
        if args.leaderboard:
            print_leaderboard(conn)
        conn.close()
        return 0
    except sqlite3.Error as exc:
        print(json.dumps({"status": "error", "message": str(exc)}), file=sys.stderr)
        return 1


if __name__ == "__main__":
    sys.exit(main())
