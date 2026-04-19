#include "nsbench/masstree_runtime.h"

#include "compiler.hh"
#include "kvthread.hh"
#include "timestamp.hh"

relaxed_atomic<mrcu_epoch_type> globalepoch(1);
relaxed_atomic<mrcu_epoch_type> active_epoch(1);
volatile bool recovering = false;
kvtimestamp_t initial_timestamp;

namespace nsbench {

threadinfo* CreateMainThreadInfo() {
    initial_timestamp = timestamp();
    return threadinfo::make(threadinfo::TI_MAIN, 0);
}

threadinfo* CreateWorkerThreadInfo(int index) {
    return threadinfo::make(threadinfo::TI_PROCESS, index);
}

void DestroyThreadInfo(threadinfo* ti) {
    (void) ti;
}

}  // namespace nsbench
