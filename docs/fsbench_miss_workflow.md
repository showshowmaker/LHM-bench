# fsbench_miss 使用说明

本文档说明如何运行 `fsbench_miss`，并分析 Ext4 与 LHM 在热态和冷态下的路径解析/小 IO 延迟。

## 1. 实验目标

`fsbench_miss` 用于比较：

- Ext4 在 `warm` 状态下的路径解析或小 IO 延迟
- Ext4 在 `cold_dropcache` 状态下的路径解析或小 IO 延迟
- LHM 在相同 namespace 下的路径解析延迟

当前支持两种模式：

- `warm`
  - 不主动清缓存，直接执行查询
- `cold_dropcache`
  - 仅对 Ext4 在运行前执行 `sync + drop_caches=3`

## 2. 编译

在 Linux 上执行：

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j"$(nproc)"
```

编译成功后可执行文件为：

- `build/fsbench_miss`

## 3. 运行单次实验

同时运行 Ext4 和 LHM，测试热态 lookup：

```bash
./build/fsbench_miss \
  --backend both \
  --mode warm \
  --op lookup \
  --query-kind positive \
  --mount-root /mnt/fsbench_ext4 \
  --output-csv ./results/fsbench_miss.csv \
  --depth 8 \
  --siblings-per-dir 16 \
  --files-per-leaf 64
```

运行冷态 Ext4 路径解析：

```bash
./build/fsbench_miss \
  --backend ext4 \
  --mode cold_dropcache \
  --op lookup \
  --query-kind positive \
  --mount-root /mnt/fsbench_ext4 \
  --output-csv ./results/fsbench_miss_ext4_cold.csv
```

运行小读：

```bash
./build/fsbench_miss \
  --backend both \
  --mode warm \
  --op read4k \
  --query-kind positive \
  --mount-root /mnt/fsbench_ext4 \
  --output-csv ./results/fsbench_miss_read4k.csv
```

运行 negative lookup：

```bash
./build/fsbench_miss \
  --backend both \
  --mode warm \
  --op negative_lookup \
  --query-kind negative \
  --mount-root /mnt/fsbench_ext4 \
  --output-csv ./results/fsbench_miss_negative.csv
```

## 4. 主要参数

- `--backend ext4|lhm|both`
- `--mode warm|cold_dropcache`
- `--op lookup|read4k|write4k|negative_lookup`
- `--query-kind positive|negative`
- `--mount-root`
  - Ext4 挂载点，`ext4` 或 `both` 时必填
- `--output-csv`
- `--depth`
- `--siblings-per-dir`
- `--files-per-leaf`
- `--positive-queries`
- `--negative-queries`
- `--seed`

## 5. CSV 字段

`fsbench_miss` 输出字段如下：

- `backend`
- `mode`
- `op`
- `query_kind`
- `query_count`
- `file_count`
- `depth`
- `siblings_per_dir`
- `files_per_leaf`
- `avg_ns`
- `p50_ns`
- `p95_ns`
- `p99_ns`
- `avg_bytes`
- `success_rate`

## 6. 分析并画图

分析脚本：

- `scripts/analyze_fsbench_miss.py`

分析单个 CSV：

```bash
python3 ./scripts/analyze_fsbench_miss.py \
  --csv ./results/fsbench_miss.csv
```

分析多个 CSV：

```bash
python3 ./scripts/analyze_fsbench_miss.py \
  --glob "./results/fsbench_miss*.csv"
```

默认输出目录：

```text
./results/fsbench_miss_analysis/
```

## 7. 分析脚本输出

脚本会生成：

- `fsbench_miss_summary.csv`
- `fsbench_miss_<op>_<query_kind>.png`

图中展示：

- 不同 backend 和 mode 下的 `p50` / `p99` 延迟
- 对应实验的 `success_rate`

## 8. 结果怎么看

建议重点看：

- `lookup + positive`
  - 比较路径解析延迟
- `negative_lookup + negative`
  - 比较不存在路径时的处理代价
- `read4k` / `write4k`
  - 作为补充展示小 IO 开销

如果结果符合预期，通常应看到：

- Ext4 在 `cold_dropcache` 下的 `p99_ns` 明显高于 `warm`
- LHM 的路径解析延迟曲线相对更稳定
