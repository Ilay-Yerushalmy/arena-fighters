-- Arena Fighters - relational schema (SQLite)
-- Idempotent: safe to run on every game start.

PRAGMA foreign_keys = ON;

-- ---------------------------------------------------------------------------
-- Tables
-- ---------------------------------------------------------------------------

CREATE TABLE IF NOT EXISTS players (
    id          INTEGER PRIMARY KEY AUTOINCREMENT,
    name        TEXT    NOT NULL UNIQUE COLLATE NOCASE,
    created_at  TEXT    NOT NULL DEFAULT (datetime('now'))
);

CREATE TABLE IF NOT EXISTS characters (
    id             INTEGER PRIMARY KEY,
    name           TEXT NOT NULL UNIQUE,
    skill_name     TEXT NOT NULL,
    ultimate_name  TEXT NOT NULL
);

CREATE TABLE IF NOT EXISTS matches (
    id               INTEGER PRIMARY KEY AUTOINCREMENT,
    player1_id       INTEGER NOT NULL REFERENCES players(id),
    player2_id       INTEGER NOT NULL REFERENCES players(id),
    character1_id    INTEGER NOT NULL REFERENCES characters(id),
    character2_id    INTEGER NOT NULL REFERENCES characters(id),
    winner_id        INTEGER REFERENCES players(id),           -- NULL = draw
    duration_sec     REAL    NOT NULL CHECK (duration_sec >= 0),
    p1_hp_pct        REAL    NOT NULL CHECK (p1_hp_pct BETWEEN 0 AND 100),
    p2_hp_pct        REAL    NOT NULL CHECK (p2_hp_pct BETWEEN 0 AND 100),
    rated            INTEGER NOT NULL DEFAULT 0,               -- set to 1 by ranking.py
    p1_rating_delta  REAL,
    p2_rating_delta  REAL,
    played_at        TEXT    NOT NULL DEFAULT (datetime('now')),
    CHECK (player1_id <> player2_id),
    CHECK (winner_id IS NULL OR winner_id IN (player1_id, player2_id))
);

CREATE TABLE IF NOT EXISTS player_stats (
    player_id       INTEGER PRIMARY KEY REFERENCES players(id) ON DELETE CASCADE,
    wins            INTEGER NOT NULL DEFAULT 0,
    losses          INTEGER NOT NULL DEFAULT 0,
    draws           INTEGER NOT NULL DEFAULT 0,
    matches_played  INTEGER NOT NULL DEFAULT 0,
    rating          REAL    NOT NULL DEFAULT 1000,
    rank            INTEGER,                                   -- NULL until first rated match
    updated_at      TEXT    NOT NULL DEFAULT (datetime('now'))
);

CREATE TABLE IF NOT EXISTS rating_history (
    id             INTEGER PRIMARY KEY AUTOINCREMENT,
    player_id      INTEGER NOT NULL REFERENCES players(id) ON DELETE CASCADE,
    match_id       INTEGER NOT NULL REFERENCES matches(id) ON DELETE CASCADE,
    rating_before  REAL    NOT NULL,
    rating_after   REAL    NOT NULL,
    created_at     TEXT    NOT NULL DEFAULT (datetime('now'))
);

-- ---------------------------------------------------------------------------
-- Indexes
-- ---------------------------------------------------------------------------

CREATE INDEX IF NOT EXISTS idx_matches_rated   ON matches(rated);
CREATE INDEX IF NOT EXISTS idx_matches_player1 ON matches(player1_id);
CREATE INDEX IF NOT EXISTS idx_matches_player2 ON matches(player2_id);
CREATE INDEX IF NOT EXISTS idx_stats_rating    ON player_stats(rating DESC);
CREATE INDEX IF NOT EXISTS idx_history_player  ON rating_history(player_id);

-- ---------------------------------------------------------------------------
-- Triggers
-- ---------------------------------------------------------------------------

-- Every new player automatically gets a stats row.
CREATE TRIGGER IF NOT EXISTS trg_players_create_stats
AFTER INSERT ON players
BEGIN
    INSERT INTO player_stats (player_id) VALUES (NEW.id);
END;

-- Every recorded match automatically updates wins / losses / draws for both players.
CREATE TRIGGER IF NOT EXISTS trg_matches_update_stats
AFTER INSERT ON matches
BEGIN
    UPDATE player_stats
       SET matches_played = matches_played + 1,
           wins   = wins   + CASE WHEN NEW.winner_id = player_id THEN 1 ELSE 0 END,
           losses = losses + CASE WHEN NEW.winner_id IS NOT NULL AND NEW.winner_id <> player_id THEN 1 ELSE 0 END,
           draws  = draws  + CASE WHEN NEW.winner_id IS NULL THEN 1 ELSE 0 END,
           updated_at = datetime('now')
     WHERE player_id IN (NEW.player1_id, NEW.player2_id);
END;

-- ---------------------------------------------------------------------------
-- Views
-- ---------------------------------------------------------------------------

CREATE VIEW IF NOT EXISTS leaderboard AS
SELECT p.id              AS player_id,
       p.name            AS name,
       s.wins            AS wins,
       s.losses          AS losses,
       s.draws           AS draws,
       s.matches_played  AS matches_played,
       s.rating          AS rating,
       s.rank            AS rank,
       CASE WHEN s.matches_played = 0 THEN 0.0
            ELSE ROUND(100.0 * s.wins / s.matches_played, 1) END AS win_rate
  FROM players p
  JOIN player_stats s ON s.player_id = p.id
 ORDER BY s.rank IS NULL, s.rank, s.rating DESC;

-- Pick rate and win rate per character (useful for game balancing).
CREATE VIEW IF NOT EXISTS character_stats AS
WITH picks AS (
    SELECT character1_id AS character_id, player1_id AS player_id, winner_id FROM matches
    UNION ALL
    SELECT character2_id, player2_id, winner_id FROM matches
)
SELECT c.id,
       c.name,
       COUNT(picks.character_id) AS times_picked,
       SUM(CASE WHEN picks.winner_id = picks.player_id THEN 1 ELSE 0 END) AS wins,
       CASE WHEN COUNT(picks.character_id) = 0 THEN 0.0
            ELSE ROUND(100.0 * SUM(CASE WHEN picks.winner_id = picks.player_id THEN 1 ELSE 0 END)
                       / COUNT(picks.character_id), 1) END AS win_rate
  FROM characters c
  LEFT JOIN picks ON picks.character_id = c.id
 GROUP BY c.id, c.name;
