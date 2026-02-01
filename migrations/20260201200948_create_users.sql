CREATE TABLE users (
id INTEGER PRIMARY KEY AUTOINCREMENT,
username TEXT NOT NULL UNIQUE,
password_hash TEXT NOT NULL,
created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
role TEXT NOT NULL DEFAULT 'poster',
CHECK (role IN ('poster', 'admin', 'banned'))
) ;
