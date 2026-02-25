CREATE TABLE refresh_blacklist (
id INTEGER PRIMARY KEY AUTOINCREMENT,
token TEXT NOT NULL UNIQUE
) ;

CREATE TABLE access_blacklist (
id INTEGER PRIMARY KEY AUTOINCREMENT,
token TEXT NOT NULL UNIQUE
) ;

CREATE INDEX idx_refresh_blacklist_token ON refresh_blacklist(token);
CREATE INDEX idx_access_blacklist_token ON access_blacklist(token);
