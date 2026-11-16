-- Character catalog. IDs must match src/Characters.cpp.
INSERT OR IGNORE INTO characters (id, name, skill_name, ultimate_name) VALUES
    (1, 'Blaze', 'Fireball',       'Inferno'),
    (2, 'Frost', 'Ice Shard',      'Blizzard'),
    (3, 'Titan', 'Ground Slam',    'Unstoppable'),
    (4, 'Shade', 'Shadow Step',    'Assassinate'),
    (5, 'Volt',  'Lightning Dash', 'Overcharge'),
    (6, 'Sage',  'Mend',           'Sanctuary'),
    (7, 'Ronin', 'Iron Guard',     'Thousand Cuts'),
    (8, 'Viper', 'Poison Dart',    'Venom Cloud');
