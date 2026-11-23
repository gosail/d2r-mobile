#include "../dm_dualmerc_ui_channel.h"
#include <assert.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>

typedef struct {
    dm_dualmerc_ui_channel *channel;
    uint64_t stale_nonce;
} stale_context;

static void *spam_stale_callback(void *arg) {
    const stale_context *ctx=(const stale_context *)arg;
    for (unsigned i=0;i<12000;i++) {
        (void)dm_dualmerc_ui_channel_answer(ctx->channel,ctx->stale_nonce,
                                            (int)(i&1));
        (void)dm_dualmerc_ui_channel_cancel(ctx->channel,ctx->stale_nonce);
    }
    return NULL;
}

int main(void) {
    dm_dualmerc_ui_channel c=DM_DUALMERC_UI_CHANNEL_INIT;
    int answer=77;
    assert(!dm_dualmerc_ui_channel_begin(NULL,1));
    assert(!dm_dualmerc_ui_channel_begin(&c,0));
    assert(dm_dualmerc_ui_channel_begin(&c,1));
    assert(!dm_dualmerc_ui_channel_begin(&c,1)); /* no nonce reuse */
    assert(dm_dualmerc_ui_channel_pending(&c,1));
    assert(!dm_dualmerc_ui_channel_poll(&c,1,&answer));
    assert(answer==77);
    assert(dm_dualmerc_ui_channel_answer(&c,1,0));
    assert(!dm_dualmerc_ui_channel_answer(&c,1,1));
    assert(!dm_dualmerc_ui_channel_pending(&c,1));
    assert(dm_dualmerc_ui_channel_poll(&c,1,&answer) && answer==0);
    assert(!dm_dualmerc_ui_channel_poll(&c,1,&answer));
    assert(!dm_dualmerc_ui_channel_cancel(&c,1));

    assert(dm_dualmerc_ui_channel_begin(&c,2));
    assert(!dm_dualmerc_ui_channel_answer(&c,1,1));
    assert(!dm_dualmerc_ui_channel_cancel(&c,1));
    assert(!dm_dualmerc_ui_channel_answer(&c,2,2));
    assert(dm_dualmerc_ui_channel_answer(&c,2,-1));
    assert(dm_dualmerc_ui_channel_poll(&c,2,&answer) && answer==-1);

    assert(dm_dualmerc_ui_channel_begin(&c,3));
    assert(dm_dualmerc_ui_channel_cancel(&c,3));
    assert(!dm_dualmerc_ui_channel_poll(&c,3,&answer));
    assert(!dm_dualmerc_ui_channel_answer(&c,3,0));

    assert(dm_dualmerc_ui_channel_begin(&c,4));
    stale_context ctx={&c,3};
    pthread_t thread;
    assert(pthread_create(&thread,NULL,spam_stale_callback,&ctx)==0);
    for (uint64_t n=5;n<200;n++) {
        assert(dm_dualmerc_ui_channel_begin(&c,n));
        assert(!dm_dualmerc_ui_channel_cancel(&c,3));
        assert(!dm_dualmerc_ui_channel_answer(&c,3,0));
        assert(dm_dualmerc_ui_channel_pending(&c,n));
    }
    assert(pthread_join(thread,NULL)==0);
    assert(dm_dualmerc_ui_channel_answer(&c,199,1));
    assert(dm_dualmerc_ui_channel_poll(&c,199,&answer) && answer==1);
    assert(!dm_dualmerc_ui_channel_poll(&c,199,&answer));
    assert(pthread_mutex_destroy(&c.mutex)==0);
    puts("dualmerc iPad chooser concurrent stale-nonce protection: passed");
    return 0;
}
