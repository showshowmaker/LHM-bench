# nsbench Linux 运行流程

本文档说明在 Linux 或 WSL 环境下，如何完整完成 `nsbench` 的深度敏感性实验流程，包括：

- 编译 benchmark 程序
- 生成不同目录深度的数据集
- 运行 LHM 与迭代式基线
- 查看输出结果 CSV

目标是产出两份可直接对比的深度扫描结果：

- `results/masstree_depth_sweep.csv`
- `results/rocksdb_depth_sweep.csv`

## 1. 前置条件

下面的说明默认满足以下条件：

- 项目根目录为 `Zyb`
- 运行环境为 Linux 或 WSL
- `third_party/rocksdb` 下已经放好了可用的 RocksDB 源码，并且该目录内包含 `CMakeLists.txt`

先安装基础构建依赖。

Ubuntu 或 Debian 上可执行：

```bash
sudo apt update
sudo apt install -y build-essential cmake git python3
```

如果你要比较迭代式 RocksDB 基线，需要把 RocksDB 放在下面这个位置：

```text
Zyb/
  third_party/
    rocksdb/
```

如果没有 RocksDB，`nsbench_run` 仍然可以构建 Masstree 后端，但 `rocksdb` 后端不会启用。

## 2. 编译

在项目根目录执行：

```bash
cd /path/to/Zyb
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j"$(nproc)"
```

编译成功后，主要可执行文件包括：

- `build/nsbench_build_dataset`
- `build/nsbench_run`
- `build/flatbench_prefix`
- `build/flatbench_churn`
- `build/flatbench_rename`

对于“路径解析深度敏感性”实验，核心只需要：

- `build/nsbench_build_dataset`
- `build/nsbench_run`

## 3. 生成数据集

按不同目录深度生成一组数据集。

示例：

```bash
mkdir -p datasets results

./build/nsbench_build_dataset \
  --output-root ./datasets \
  --depths 1,2,4,8,16,32,64 \
  --siblings-per-dir 256 \
  --files-per-leaf 256 \
  --positive-queries 50000 \
  --negative-queries 50000
```

执行后会生成：

```text
datasets/
  depth_01/
    manifest.txt
    records.tsv
    positive_queries.tsv
    negative_queries.tsv
  depth_02/
  depth_04/
  ...
  depth_64/
```

每个 `manifest.txt` 对应一组独立数据集，它是 `nsbench_run` 的直接输入。

## 4. 手动运行单个深度

如果你想先验证某一个深度，可以直接指定单个 manifest。

LHM 示例：

```bash
./build/nsbench_run \
  --backend masstree \
  --manifest ./datasets/depth_08/manifest.txt \
  --warmup 10000 \
  --repeats 10 \
  --output-csv ./results/masstree_depth08.csv
```

迭代式基线示例：

```bash
./build/nsbench_run \
  --backend rocksdb \
  --manifest ./datasets/depth_08/manifest.txt \
  --warmup 10000 \
  --repeats 10 \
  --output-csv ./results/rocksdb_depth08.csv
```

如果还要同时测 negative lookup，可加上：

```bash
./build/nsbench_run \
  --backend masstree \
  --manifest ./datasets/depth_08/manifest.txt \
  --warmup 10000 \
  --repeats 10 \
  --include-negative \
  --output-csv ./results/masstree_depth08_with_negative.csv
```

## 5. 运行完整深度扫描

推荐做法是把所有深度一次性扫完，并让 `nsbench_run` 为每个 backend 各输出一份汇总 CSV。

### 方式 A：使用自带 Bash 脚本

```bash
bash ./scripts/run_nsbench_depth_sweep.sh ./build/nsbench_run ./datasets ./results
```

该脚本默认会：

- 扫描 `./datasets/depth_*/manifest.txt`
- 运行 `masstree`
- 运行 `rocksdb`
- 输出：
  - `results/masstree_depth_sweep.csv`
  - `results/rocksdb_depth_sweep.csv`

你也可以通过环境变量覆盖默认参数：

```bash
WARMUP=10000 \
REPEATS=10 \
BACKENDS="masstree rocksdb" \
INCLUDE_NEGATIVE=0 \
NO_VERIFY=0 \
bash ./scripts/run_nsbench_depth_sweep.sh ./build/nsbench_run ./datasets ./results
```

### 方式 B：直接使用 `nsbench_run` 配合 manifest 列表

先生成 manifest 列表文件：

```bash
find ./datasets -mindepth 2 -maxdepth 2 -name manifest.txt | sort > ./results/manifest_list.txt
```

然后运行 LHM：

```bash
./build/nsbench_run \
  --backend masstree \
  --manifest-list ./results/manifest_list.txt \
  --warmup 10000 \
  --repeats 10 \
  --output-csv ./results/masstree_depth_sweep.csv
```

再运行迭代式基线：

```bash
./build/nsbench_run \
  --backend rocksdb \
  --manifest-list ./results/manifest_list.txt \
  --warmup 10000 \
  --repeats 10 \
  --output-csv ./results/rocksdb_depth_sweep.csv
```

你也可以直接重复传多个 `--manifest`：

```bash
./build/nsbench_run \
  --backend masstree \
  --manifest ./datasets/depth_01/manifest.txt \
  --manifest ./datasets/depth_02/manifest.txt \
  --manifest ./datasets/depth_04/manifest.txt \
  --manifest ./datasets/depth_08/manifest.txt \
  --output-csv ./results/masstree_partial_sweep.csv
```

## 6. 结果文件在哪里

主要结果文件在你通过 `--output-csv` 或批量脚本指定的输出目录中。

典型目录结构如下：

```text
results/
  manifest_list.txt
  masstree_depth_sweep.csv
  rocksdb_depth_sweep.csv
```

如果你手动跑了单个深度，也可能看到：

```text
results/
  masstree_depth08.csv
  rocksdb_depth08.csv
```

原始数据集文件保留在：

```text
datasets/depth_01/
datasets/depth_02/
...
datasets/depth_64/
```

## 7. 如何查看结果

### 终端快速查看

查看前几行：

```bash
head -n 5 ./results/masstree_depth_sweep.csv
head -n 5 ./results/rocksdb_depth_sweep.csv
```

按 `positive` 查询过滤：

```bash
grep ",positive," ./results/masstree_depth_sweep.csv | head
grep ",positive," ./results/rocksdb_depth_sweep.csv | head
```

如果系统安装了 `csvlook`，也可以更直观地看：

```bash
csvlook ./results/masstree_depth_sweep.csv | less -S
csvlook ./results/rocksdb_depth_sweep.csv | less -S
```

### 用 Python 或 Pandas 查看

```bash
python3 - <<'PY'
import pandas as pd

lhm = pd.read_csv("results/masstree_depth_sweep.csv")
itv = pd.read_csv("results/rocksdb_depth_sweep.csv")

print("LHM")
print(lhm[["depth", "query_kind", "avg_ns", "p50_ns", "p99_ns", "avg_index_steps"]].head())

print("Iterative")
print(itv[["depth", "query_kind", "avg_ns", "p50_ns", "p99_ns", "avg_index_steps"]].head())
PY
```

## 8. 哪些字段最重要

对于“路径解析深度敏感性”结论，重点关注：

- `depth`
- `avg_ns`
- `p50_ns`
- `p99_ns`
- `avg_component_steps`
- `avg_index_steps`

解释方式：

- 迭代式基线：
  - `avg_index_steps` 应当随 `depth` 近似线性增长
  - 延迟曲线会表现出明显正斜率
- LHM：
  - `avg_index_steps` 增长应远小于迭代式，甚至接近常数
  - 延迟斜率应显著更小

完整字段定义见：

- [nsbench_csv_fields.md](./nsbench_csv_fields.md)

## 9. 最小画图示例

下面这个 Python 片段会画出 `positive lookup` 下的 `p50_ns` 与 `p99_ns` 随目录深度的变化。

```bash
python3 - <<'PY'
import pandas as pd
import matplotlib.pyplot as plt

lhm = pd.read_csv("results/masstree_depth_sweep.csv")
itv = pd.read_csv("results/rocksdb_depth_sweep.csv")

lhm = lhm[lhm["query_kind"] == "positive"].groupby("depth", as_index=False).mean(numeric_only=True)
itv = itv[itv["query_kind"] == "positive"].groupby("depth", as_index=False).mean(numeric_only=True)

plt.figure(figsize=(8, 5))
plt.plot(itv["depth"], itv["p50_ns"], marker="o", label="Iterative p50")
plt.plot(lhm["depth"], lhm["p50_ns"], marker="o", label="LHM p50")
plt.plot(itv["depth"], itv["p99_ns"], marker="s", label="Iterative p99")
plt.plot(lhm["depth"], lhm["p99_ns"], marker="s", label="LHM p99")
plt.xlabel("Depth")
plt.ylabel("Latency (ns)")
plt.legend()
plt.tight_layout()
plt.savefig("results/depth_sensitivity.png", dpi=200)
print("wrote results/depth_sensitivity.png")
PY
```

生成的图会在：

- `results/depth_sensitivity.png`

## 10. 推荐的完整实验命令

下面是一套标准的完整流程：

```bash
cd /path/to/Zyb

cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j"$(nproc)"

mkdir -p datasets results

./build/nsbench_build_dataset \
  --output-root ./datasets \
  --depths 1,2,4,8,16,32,64 \
  --siblings-per-dir 256 \
  --files-per-leaf 256 \
  --positive-queries 50000 \
  --negative-queries 50000

bash ./scripts/run_nsbench_depth_sweep.sh ./build/nsbench_run ./datasets ./results
```

然后重点查看：

- `results/masstree_depth_sweep.csv`
- `results/rocksdb_depth_sweep.csv`
- 可选的绘图结果 `results/depth_sensitivity.png`

这就是从编译、生成数据、批量运行到查看结果的完整流程。
