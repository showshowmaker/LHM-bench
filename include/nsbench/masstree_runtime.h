#pragma once

struct threadinfo;

namespace nsbench {

threadinfo* CreateMainThreadInfo();
threadinfo* CreateWorkerThreadInfo(int index);
void DestroyThreadInfo(threadinfo* ti);

}  // namespace nsbench
