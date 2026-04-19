# nsbench 结果分析

本说明用于分析 `nsbench` 深度扫描实验的输出结果，并将结果画成图。

分析脚本：

- `scripts/analyze_nsbench_results.py`

该脚本默认读取：

- `root/VSIterate/masstree_depth_sweep.csv`
- `root/VSIterate/rocksdb_depth_sweep.csv`

并输出：

- 聚合后的 summary CSV
- 斜率统计 CSV
- 延迟图
- 步数图

## 1. 依赖

需要 Python 3 和 `matplotlib`。

在 Linux 上可执行：

```bash
python3 -m pip install matplotlib
```

## 2. 基本用法

假设你已经跑完完整实验，结果都在：

```text
./root/VSIterate/
```

则直接执行：

```bash
python3 ./scripts/analyze_nsbench_results.py \
  --suite-dir ./root/VSIterate
```

默认分析 `positive` 查询，并把结果输出到：

```text
./results/
```

## 3. 指定分析 negative 查询

```bash
python3 ./scripts/analyze_nsbench_results.py \
  --suite-dir ./root/VSIterate \
  --query-kind negative
```

## 4. 指定输出目录

```bash
python3 ./scripts/analyze_nsbench_results.py \
  --suite-dir ./root/VSIterate \
  --output-dir ./results/analysis_positive
```

## 5. 输出文件说明

以 `positive` 查询为例，脚本会生成：

- `analysis/rocksdb_positive_summary.csv`
- `analysis/masstree_positive_summary.csv`
- `analysis/positive_slopes.csv`
- `analysis/positive_depth_latency.png`
- `analysis/positive_depth_avg_latency.png`
- `analysis/positive_depth_steps.png`

含义如下：

- `rocksdb_positive_summary.csv`
  - 将 RocksDB 迭代式结果按 `depth` 聚合后的汇总表
- `masstree_positive_summary.csv`
  - 将 LHM 结果按 `depth` 聚合后的汇总表
- `positive_slopes.csv`
  - 对 `avg_ns`、`p50_ns`、`p99_ns`、`avg_index_steps` 做线性拟合后的斜率
- `positive_depth_latency.png`
  - 同时展示 `p50` 和 `p99` 随深度变化的图
- `positive_depth_avg_latency.png`
  - 展示平均延迟随深度变化的图
- `positive_depth_steps.png`
  - 左图是 `avg_component_steps`
  - 右图是 `avg_index_steps`

## 6. 如何解读结果

这组实验最关键看三点：

- 迭代式的 `p50_ns` 和 `p99_ns` 是否随着 `depth` 明显上升
- 迭代式的 `avg_index_steps` 是否近似随 `depth` 线性增长
- LHM 的 `avg_index_steps` 和延迟曲线是否更平缓

如果结果符合预期，应当看到：

- RocksDB 迭代式的延迟和 `avg_index_steps` 随深度增加明显变大
- LHM 的 `avg_component_steps` 可能增长，但 `avg_index_steps` 增长更慢
- 因此 LHM 的延迟曲线比迭代式更平

## 7. 推荐查看顺序

建议先看：

- `positive_depth_latency.png`
- `positive_depth_steps.png`

然后再看：

- `positive_slopes.csv`

其中：

- `p50_ns` / `p99_ns` 的斜率反映延迟的深度敏感性
- `avg_index_steps` 的斜率反映索引访问步数的深度敏感性

如果 LHM 的斜率显著小于迭代式，就能支持你的核心结论。
