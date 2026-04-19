#ifndef TOKEN_HElPER_H
#define TOKEN_HElPER_H

#include <Arduino.h>
#include "FirebaseFS.h"

#include <Firebase.h>

// This header file includes the functions that provide the token generation process info.

/* The helper function to get the token type string */
const char *getTokenType(struct token_info_t info);

/* The helper function to get the token status string */
const char *getTokenStatus(struct token_info_t info);

/* The helper function to get the token error string */
String getTokenError(struct token_info_t info);

void tokenStatusCallback(TokenInfo info);

#endif
