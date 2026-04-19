# nsbench CSV Fields

完整的 Linux 构建与运行流程见：

- [nsbench_linux_workflow.md](./nsbench_linux_workflow.md)

`nsbench_run` 会为每个 `(backend, manifest, query_kind, repeat)` 输出一行记录，字段如下：

- `backend`：解析后端名称，例如 `rocksdb_iterative` 或 `masstree_lhm`
- `dataset_name`：`manifest.txt` 中记录的数据集名称
- `manifest_path`：本次运行使用的 manifest 路径
- `depth`：该数据集对应的目录深度
- `query_kind`：查询类型，取值为 `positive` 或 `negative`
- `repeat`：重复实验编号，从 0 开始
- `query_count`：本轮重复实验中的查询数
- `avg_ns`：平均路径解析延迟，单位为纳秒
- `p50_ns`：50 分位延迟，单位为纳秒
- `p95_ns`：95 分位延迟，单位为纳秒
- `p99_ns`：99 分位延迟，单位为纳秒
- `throughput_qps`：本轮重复实验的查询吞吐，单位为 QPS
- `avg_depth`：本轮查询的平均路径深度
- `avg_component_steps`：平均逻辑路径组件步数
- `avg_index_steps`：平均后端索引探测步数或路由步数
- `avg_steps`：兼容字段，目前等于 `avg_index_steps`

用于“深度敏感性”实验时，推荐重点关注：

- 绘制 `p50_ns` 和 `p99_ns` 随 `depth` 的变化曲线
- 对 `avg_ns = alpha + beta * depth` 做线性拟合
- 结合 `avg_component_steps` 与 `avg_index_steps` 说明不同架构的工作量放大机制
