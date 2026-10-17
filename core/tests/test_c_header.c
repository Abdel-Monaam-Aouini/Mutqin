/* Compiled as plain C (C11, -Wall -Wextra -Wpedantic -Werror): proves that mutqin.h is a valid C header and
 * that the library can be used from C. Runs without the Quran text. */
#include <stdio.h>
#include <string.h>

#include "mutqin.h"

int main(void) {
    char err[128];
    mq_engine* engine = (mq_engine*)1;
    mq_session* session = NULL;
    mq_event event;
    mq_item item;
    mq_phoneme phoneme;

    if (mq_version() == NULL || strlen(mq_version()) == 0)
        return 1;
    if (mq_engine_create(NULL, &engine, err, sizeof err) != MQ_ERR_ARGUMENT || engine != NULL)
        return 2;
    if (mq_engine_create("/does/not/exist", &engine, err, sizeof err) != MQ_ERR_DATA || strlen(err) == 0)
        return 3;
    if (mq_session_create(NULL, 1, 1, 7, &session, err, sizeof err) != MQ_ERR_ARGUMENT)
        return 4;
    if (mq_session_event(NULL, 0, &event) != MQ_ERR_ARGUMENT || mq_session_item(NULL, 0, &item) != MQ_ERR_ARGUMENT ||
        mq_session_item_rules(NULL, 0, NULL, 0) != MQ_ERR_ARGUMENT ||
        mq_session_phoneme(NULL, 0, &phoneme) != MQ_ERR_ARGUMENT)
        return 5;
    if (mq_session_position(NULL) != -1 || mq_session_word_count(NULL) != 0)
        return 6;
    mq_session_destroy(NULL);
    mq_engine_destroy(NULL);
    printf("mutqin %s: header and C usage OK\n", mq_version());
    return 0;
}
