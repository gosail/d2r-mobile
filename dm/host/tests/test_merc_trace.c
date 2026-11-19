#include "../dm_merc_trace.h"
#include <assert.h>
#include <stdio.h>

int main(void) {
    dm_merc_trace_gear a = {0}, b = {0};
    assert(DM_MERC_GEAR_ABSENT == 0 && DM_MERC_GEAR_OK == 1);
    dm_merc_trace_add(&a, 19);
    dm_merc_trace_add(&a, 23);
    dm_merc_trace_add(&a, 31);
    dm_merc_trace_add(&b, 31);
    dm_merc_trace_add(&b, 19);
    dm_merc_trace_add(&b, 23);
    assert(a.count == 3 && b.count == 3);
    assert(a.fingerprint == b.fingerprint);
    dm_merc_trace_add(&b, 37);
    assert(a.fingerprint != b.fingerprint && b.count == 4);
    dm_merc_trace_add(NULL, 100);
    puts("dualmerc read-only gear snapshot helpers: all assertions passed");
    return 0;
}
