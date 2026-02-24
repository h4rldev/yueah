CREATE TABLE users (
id INTEGER PRIMARY KEY AUTOINCREMENT,
userid TEXT NOT NULL UNIQUE, -- for jwt reasons
username TEXT NOT NULL UNIQUE,
password_hash TEXT NOT NULL,
created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
role TEXT NOT NULL DEFAULT 'poster',
CHECK (role IN ('poster', 'admin', 'banned'))
) ;

CREATE INDEX idx_users_created_at ON users(created_at);
CREATE INDEX idx_users_role ON users(role);
CREATE INDEX idx_users_role_created ON users(role, created_at);
