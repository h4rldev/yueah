#ifndef DOTENV_H
#define DOTENV_H

/*
 * Get env vars from a file
 *
 * [path]  Location of env file
 *
 * Returns amount of env vars loaded
 */
int load_dotenv(const char *path);

#endif // !DOTENV_H
