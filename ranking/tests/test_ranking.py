"""Unit + integration tests for the ranking service and the SQL triggers.

Run from the project root:
    python -m unittest discover -s ranking/tests -v
"""

import sqlite3
import sys
import unittest
from pathlib import Path

PROJECT_ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(PROJECT_ROOT / "ranking"))

import ranking  # noqa: E402


def make_db() -> sqlite3.Connection:
    conn = sqlite3.connect(":memory:")
    conn.executescript((PROJECT_ROOT / "db" / "schema.sql").read_text(encoding="utf-8"))
    conn.executescript((PROJECT_ROOT / "db" / "seed.sql").read_text(encoding="utf-8"))
    return conn


def add_player(conn, name):
    return conn.execute("INSERT INTO players (name) VALUES (?)", (name,)).lastrowid


def add_match(conn, p1, p2, winner, p1_hp=50.0, p2_hp=0.0):
    conn.execute(
        "INSERT INTO matches (player1_id, player2_id, character1_id, character2_id, winner_id,"
        " duration_sec, p1_hp_pct, p2_hp_pct) VALUES (?, ?, 1, 2, ?, 60, ?, ?)",
        (p1, p2, winner, p1_hp, p2_hp),
    )


def stats(conn, player_id):
    return conn.execute(
        "SELECT wins, losses, draws, matches_played, rating, rank FROM player_stats WHERE player_id = ?",
        (player_id,),
    ).fetchone()


class EloMathTest(unittest.TestCase):
    def test_equal_ratings_expect_half(self):
        self.assertAlmostEqual(ranking.expected_score(1000, 1000), 0.5)

    def test_change_is_zero_sum(self):
        change = ranking.compute_rating_change(1100, 950, 1.0, 40)
        self.assertAlmostEqual(change.delta_p1 + change.delta_p2, 0.0)

    def test_upset_win_gains_more_than_expected_win(self):
        upset = ranking.compute_rating_change(900, 1100, 1.0, 50)
        expected = ranking.compute_rating_change(1100, 900, 1.0, 50)
        self.assertGreater(upset.delta_p1, expected.delta_p1)

    def test_dominant_win_gains_more(self):
        close = ranking.compute_rating_change(1000, 1000, 1.0, 5)
        flawless = ranking.compute_rating_change(1000, 1000, 1.0, 100)
        self.assertGreater(flawless.delta_p1, close.delta_p1)

    def test_draw_between_equals_changes_nothing(self):
        change = ranking.compute_rating_change(1000, 1000, 0.5, None)
        self.assertAlmostEqual(change.delta_p1, 0.0)


class TriggerTest(unittest.TestCase):
    def setUp(self):
        self.conn = make_db()
        self.a = add_player(self.conn, "Ilay")
        self.b = add_player(self.conn, "Dana")

    def test_new_player_gets_stats_row(self):
        self.assertEqual(stats(self.conn, self.a)[:4], (0, 0, 0, 0))

    def test_win_updates_both_players(self):
        add_match(self.conn, self.a, self.b, self.a)
        self.assertEqual(stats(self.conn, self.a)[:4], (1, 0, 0, 1))
        self.assertEqual(stats(self.conn, self.b)[:4], (0, 1, 0, 1))

    def test_draw_updates_draws(self):
        add_match(self.conn, self.a, self.b, None, 30, 30)
        self.assertEqual(stats(self.conn, self.a)[:4], (0, 0, 1, 1))
        self.assertEqual(stats(self.conn, self.b)[:4], (0, 0, 1, 1))

    def test_winner_must_be_a_participant(self):
        outsider = add_player(self.conn, "Outsider")
        with self.assertRaises(sqlite3.IntegrityError):
            add_match(self.conn, self.a, self.b, outsider)


class RankingServiceTest(unittest.TestCase):
    def setUp(self):
        self.conn = make_db()
        self.a = add_player(self.conn, "Ilay")
        self.b = add_player(self.conn, "Dana")
        self.c = add_player(self.conn, "Noam")

    def test_winner_ranked_first_and_ratings_conserved(self):
        add_match(self.conn, self.a, self.b, self.a)
        processed = ranking.update_rankings(self.conn)

        self.assertEqual(processed, 1)
        a, b = stats(self.conn, self.a), stats(self.conn, self.b)
        self.assertGreater(a[4], 1000)
        self.assertLess(b[4], 1000)
        self.assertAlmostEqual(a[4] + b[4], 2000)
        self.assertEqual((a[5], b[5]), (1, 2))

    def test_players_without_matches_stay_unranked(self):
        add_match(self.conn, self.a, self.b, self.a)
        ranking.update_rankings(self.conn)
        self.assertIsNone(stats(self.conn, self.c)[5])

    def test_running_twice_is_idempotent(self):
        add_match(self.conn, self.a, self.b, self.b)
        ranking.update_rankings(self.conn)
        first = stats(self.conn, self.b)
        self.assertEqual(ranking.update_rankings(self.conn), 0)
        self.assertEqual(stats(self.conn, self.b), first)

    def test_history_is_recorded(self):
        add_match(self.conn, self.a, self.b, self.a)
        ranking.update_rankings(self.conn)
        count = self.conn.execute("SELECT COUNT(*) FROM rating_history").fetchone()[0]
        self.assertEqual(count, 2)

    def test_tied_ratings_share_rank(self):
        add_match(self.conn, self.a, self.b, None, 50, 50)
        ranking.update_rankings(self.conn)
        self.assertEqual(stats(self.conn, self.a)[5], stats(self.conn, self.b)[5])


if __name__ == "__main__":
    unittest.main()
