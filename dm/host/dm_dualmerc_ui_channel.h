/* Thread-safe one-shot answer channel for the offline two-merc iPad picker.
 *
 * This is NOT a native hire transaction. UI callbacks must never be allowed
 * to decide a newer NPC hire after their original request was superseded.
 * Store nonce + result under ONE mutex; independently atomic fields cannot
 * protect the check-then-write window between UIKit and the game thread.
 */
#ifndef DM_DUALMERC_UI_CHANNEL_H
#define DM_DUALMERC_UI_CHANNEL_H
#include <pthread.h>
#include <stdint.h>
#define DM_DUALMERC_UI_PENDING (-2)

typedef struct {
    pthread_mutex_t mutex;
    uint64_t nonce;
    uint64_t last_nonce;
    int answer;
} dm_dualmerc_ui_channel;
#define DM_DUALMERC_UI_CHANNEL_INIT { PTHREAD_MUTEX_INITIALIZER, 0, 0, DM_DUALMERC_UI_PENDING }

static inline int dm_dualmerc_ui_channel_begin(
    dm_dualmerc_ui_channel *c, uint64_t nonce)
{
    if (!c || !nonce) return 0;
    pthread_mutex_lock(&c->mutex);
    int accepted = nonce > c->last_nonce;
    if (accepted) {
        c->nonce=nonce;
        c->last_nonce=nonce;
        c->answer=DM_DUALMERC_UI_PENDING;
    }
    pthread_mutex_unlock(&c->mutex);
    return accepted;
}

static inline int dm_dualmerc_ui_channel_pending(
    dm_dualmerc_ui_channel *c, uint64_t nonce)
{
    if (!c || !nonce) return 0;
    pthread_mutex_lock(&c->mutex);
    int pending = c->nonce == nonce && c->answer == DM_DUALMERC_UI_PENDING;
    pthread_mutex_unlock(&c->mutex);
    return pending;
}

/* -1 = cancel, 0/1 = selected slot. Ignore duplicates after first result. */
static inline int dm_dualmerc_ui_channel_answer(
    dm_dualmerc_ui_channel *c, uint64_t nonce, int answer)
{
    if (!c || !nonce || answer < -1 || answer >= 2) return 0;
    pthread_mutex_lock(&c->mutex);
    int accepted = c->nonce == nonce && c->answer == DM_DUALMERC_UI_PENDING;
    if (accepted) c->answer=answer;
    pthread_mutex_unlock(&c->mutex);
    return accepted;
}

static inline int dm_dualmerc_ui_channel_poll(
    dm_dualmerc_ui_channel *c, uint64_t nonce, int *answer)
{
    if (!c || !nonce || !answer) return 0;
    pthread_mutex_lock(&c->mutex);
    int ready = c->nonce == nonce && c->answer != DM_DUALMERC_UI_PENDING;
    if (ready) {
        *answer=c->answer;
        c->nonce=0; c->answer=DM_DUALMERC_UI_PENDING;
    }
    pthread_mutex_unlock(&c->mutex);
    return ready;
}

static inline int dm_dualmerc_ui_channel_cancel(
    dm_dualmerc_ui_channel *c, uint64_t nonce)
{
    if (!c || !nonce) return 0;
    pthread_mutex_lock(&c->mutex);
    int cancelled = c->nonce == nonce;
    if (cancelled) {
        c->nonce=0; c->answer=DM_DUALMERC_UI_PENDING;
    }
    pthread_mutex_unlock(&c->mutex);
    return cancelled;
}
#endif
