CREATE TABLE posts (
id INTEGER PRIMARY KEY AUTOINCREMENT,
title TEXT NOT NULL,
author_id INTEGER NOT NULL,
summary TEXT NOT NULL,
content TEXT NOT NULL,
created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
FOREIGN KEY (author_id) REFERENCES users (id)
) ;

CREATE TABLE post_tags (
post_id INTEGER NOT NULL,
tag TEXT NOT NULL,
PRIMARY KEY (post_id, tag),
FOREIGN KEY (post_id) REFERENCES posts (id)
) ;

CREATE INDEX idx_posts_author_id ON posts(author_id);
CREATE INDEX idx_posts_created_at ON posts(created_at);
CREATE INDEX idx_posts_author_created ON posts(author_id, created_at);

