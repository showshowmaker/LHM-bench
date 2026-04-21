# fsbench_memory 使用说明

本文档说明如何运行 `fsbench_memory`，并分析 Ext4 与 LHM 的文件元数据缓存平均占用。

## 1. 实验目标

`fsbench_memory` 用于比较：

- Ext4 在 VFS/Ext4 元数据缓存中的平均每文件内存占用
- LHM 在用户态命名空间索引中的平均每文件元数据占用

当前程序输出三个阶段：

- `baseline`
  - 构建 workload 之前的初始快照
- `populate`
  - 创建完目录树和文件后的快照增量
- `warm`
  - 对所有路径执行一轮 lookup/stat 预热后的快照增量

## 2. 编译

在 Linux 上执行：

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j"$(nproc)"
```

编译成功后可执行文件为：

- `build/fsbench_memory`

## 3. 运行单次实验

同时运行 Ext4 和 LHM：

```bash
./build/fsbench_memory \
  --backend both \
  --mount-root /mnt/fsbench_ext4 \
  --output-csv ./results/fsbench_memory.csv \
  --depth 8 \
  --siblings-per-dir 16 \
  --files-per-leaf 64 \
  --target-file-count 1000000
```

仅运行 Ext4：

```bash
./build/fsbench_memory \
  --backend ext4 \
  --mount-root /mnt/fsbench_ext4 \
  --output-csv ./results/fsbench_memory_ext4.csv \
  --depth 8 \
  --siblings-per-dir 16 \
  --files-per-leaf 64 \
  --target-file-count 1000000
```

仅运行 LHM：

```bash
./build/fsbench_memory \
  --backend lhm \
  --output-csv ./results/fsbench_memory_lhm.csv \
  --depth 8 \
  --siblings-per-dir 16 \
  --files-per-leaf 64 \
  --target-file-count 1000000
```

## 4. 主要参数

- `--backend ext4|lhm|both`
  - 选择后端
- `--mount-root`
  - Ext4 后端挂载点路径；`ext4` 或 `both` 时必填
- `--output-csv`
  - 输出 CSV 路径
- `--depth`
  - 主链目录深度
- `--siblings-per-dir`
  - 每层附加的兄弟目录数
- `--files-per-leaf`
  - 最深叶子目录下的文件数
- `--target-file-count`
  - 总文件数目标；设置后，文件会均匀分散到多个叶子目录，而不是集中在单个最深目录
- `--seed`
  - workload 随机种子

## 5. CSV 字段

`fsbench_memory` 输出字段如下：

- `backend`
- `file_count`
- `depth`
- `siblings_per_dir`
- `files_per_leaf`
- `phase`
- `total_meta_bytes`
- `bytes_per_file`
- `slab_dentry_bytes`
- `slab_inode_bytes`
- `slab_ext4_inode_bytes`
- `lhm_index_bytes`
- `lhm_inode_bytes`
- `lhm_string_bytes`
- `process_rss_bytes`

字段解释：

- `total_meta_bytes`
  - 当前阶段的元数据内存增量
- `bytes_per_file`
  - `total_meta_bytes / file_count`
- `slab_*`
  - Ext4 侧从 `/proc/slabinfo` 采样到的细分缓存
- `lhm_*`
  - LHM 侧内部元数据记账
- `process_rss_bytes`
  - 进程 RSS 增量的近似参考值

## 6. 分析结果并画图

分析脚本：

- `scripts/analyze_fsbench_memory.py`

分析单个 CSV：

```bash
python3 ./scripts/analyze_fsbench_memory.py \
  --csv ./results/fsbench_memory.csv
```

如果你做了多组规模扫描，也可以一次分析多个 CSV：

```bash
python3 ./scripts/analyze_fsbench_memory.py \
  --glob "./results/fsbench_memory_*.csv"
```

分析结果默认输出到：

```text
./results/fsbench_memory_analysis/
```

## 7. 分析脚本输出

脚本会生成：

- `fsbench_memory_combined.csv`
- `fsbench_memory_summary.csv`
- `fsbench_memory_by_phase.png`
- `fsbench_memory_warm_trend_total_bytes.png`
- `fsbench_memory_warm_trend_bytes_per_file.png`
- `fsbench_memory_ext4_slab_breakdown.png`

其中：

- `fsbench_memory_by_phase.png`
  - 展示每个配置下，`baseline/populate/warm` 三个阶段的
    - 平均每文件元数据占用
    - 总元数据占用
- `fsbench_memory_warm_trend_total_bytes.png`
  - 展示选定阶段默认 `warm` 下，不同规模配置的总元数据占用趋势
- `fsbench_memory_warm_trend_bytes_per_file.png`
  - 展示选定阶段默认 `warm` 下，不同规模配置的平均每文件占用趋势
- `fsbench_memory_ext4_slab_breakdown.png`
  - 展示 Ext4 侧 `dentry` / `inode_cache` / `ext4_inode_cache` 的堆叠分解

## 8. 结果怎么看

这组实验最关键看两点：

- `bytes_per_file`
  - 比较平均每文件元数据占用
- `total_meta_bytes`
  - 比较整体元数据缓存压力

如果结果符合预期，应当看到：

- Ext4 的 `dentry + inode` 元数据缓存随文件数增长明显增加
- LHM 的元数据结构更紧凑，`bytes_per_file` 更低或增长更慢

对于 Ext4，还可以从 `fsbench_memory_ext4_slab_breakdown.png` 中看到：

- `dentry`
- `inode_cache`
- `ext4_inode_cache`

## Auto Cleanup

- `fsbench_memory` now clears all data under `--mount-root` before it builds the Ext4 workload.
- `fsbench_memory` also clears all data under `--mount-root` again before the process exits.
- The mount directory itself is kept. Only its children are removed.

三者在不同阶段的贡献。
