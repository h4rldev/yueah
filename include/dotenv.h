#ifndef DOTENV_H
#define DOTENV_H

/*
 * @brief Get env vars from a file
 *
 * @param path Path to the file to load env vars from
 *
 * @return Amount of loaded env vars
 */
int load_dotenv(const char *path);

#endif // !DOTENV_H
