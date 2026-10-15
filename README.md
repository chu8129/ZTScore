# ZTScore

## 备注
### 尽量C++风格的C
```
    安卓为cpp
```

### 尽量使用malloc而非数组
```
    ios会有异常情况，原因不明
```

### 切记除法需要小数时*1.0

### 日志统一: define LOGD

### 关于PCM
```
    传输是用unsight char
    储存是小端:array[2*index + 1] << 8 | array[2*index]
    由于使用的是bit, 故>if (transValue > 32767){transValue = value - 65536;}
```

### 日志vs本读读取的pcm
![image](https://github.com/chu8129/ZTScore/blob/qw/WeChatWorkScreenshot_3df7e0b7-bbd9-4f51-9d47-a3336bd6f6a1.png)
### pcm图(上图:原始pcm值,中图下图:转int16后)
![image](https://github.com/chu8129/ZTScore/blob/qw/Figure_1.png)
### 转换后的音频
[https://github.com/chu8129/ZTScore/blob/qw/pcm_0.wav](https://github.com/chu8129/ZTScore/blob/qw/pcm_0.wav)
![logo][logo]


[logo]:data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAABAAAAAQCAYAAAAf8/9hAAAAAXNSR0IArs4c6QAAAB9JREFUOE9jZKAQMFKon2HUAIbRMGAYDQNQPhr4vAAAJpgAEX/anFwAAAAASUVORK5CYII=




---
---
---

# ZTScore 项目 README

> 一个基于 PCM 音频信号处理与 DTW 动态时间规整的卡拉 OK 演唱评分算法。  
> 输入用户演唱的 PCM 数据和歌词时间轴，输出 **节奏分** 与 **情感分** 两个维度的评分。

---

## 目录

- [1. 项目简介](#1-项目简介)
- [2. 核心概念](#2-核心概念)
- [3. 整体流程图](#3-整体流程图)
- [4. 数据结构](#4-数据结构)
- [5. 模块详解](#5-模块详解)
  - [5.1 对外入口](#51-对外入口)
  - [5.2 PCM 预处理](#52-pcm-预处理)
  - [5.3 歌词时间中点计算](#53-歌词时间中点计算)
  - [5.4 非静默区间检测](#54-非静默区间检测)
  - [5.5 静默检测参数搜索](#55-静默检测参数搜索)
  - [5.6 DTW 动态时间规整](#56-dtw-动态时间规整)
  - [5.7 节奏评分](#57-节奏评分)
  - [5.8 情感评分](#58-情感评分)
- [6. 关键指标：RMS 与 dBFS](#6-关键指标rms-与-dbfs)
- [7. 完整调用链](#7-完整调用链)
- [8. 参数说明](#8-参数说明)
- [9. 已知问题与注意事项](#9-已知问题与注意事项)
- [10. 移植到 Python 的建议](#10-移植到-python-的建议)

---

## 1. 项目简介

ZTScore 是一个用 C 语言实现的**唱歌评分核心算法**，原本用于 Android 平台（通过 `__android_log_print` 输出日志）。

它的核心思想是：

1. 从用户演唱的 PCM 音频中，**检测出实际发声的段落**（非静默区间）。
2. 把这些发声段落的**时间中点**与标准歌词的**时间中点**做 **DTW 对齐**，衡量节奏吻合度。
3. 结合发声段落的**时长差异**和**音量动态范围**，评估演唱的情感表现力。

最终输出两个 `[0, 1]` 之间的分数：

| 分数 | 含义 |
|------|------|
| `res[0]` | 节奏分（Rhythm Grade） |
| `res[1]` | 情感分（Emotion Grade） |

---

## 2. 核心概念

| 概念 | 说明 |
|------|------|
| **PCM** | 脉冲编码调制，原始音频采样数据，本项目为 16-bit 小端双声道 |
| **帧（Frame）** | 一个采样点。双声道时，左右声道各一个采样点算一帧的两部分 |
| **RMS** | 均方根，衡量窗口内信号的平均能量 |
| **dBFS** | 相对满量程的分贝，`20*log10(RMS/满量程)`，范围 -∞ ~ 0 |
| **静默区间** | 音频中能量低于阈值的连续片段 |
| **非静默区间** | 用户实际发声的连续片段，即静默区间的补集 |
| **DTW** | 动态时间规整，允许时间轴弹性对齐的序列距离度量 |
| **歌词时间中点** | 每个字 `(起始时间 + 时长/2)`，作为该字的代表时刻 |

---

## 3. 整体流程图

```
┌──────────────────────────────────────────────────────────────┐
│                     输入                                      │
│  pcmList[]      : unsigned char 数组，16-bit PCM 字节流       │
│  lyricList[]    : int 数组，[t1_start, t1_dur, t2_start, ...] │
└──────────────────────────┬───────────────────────────────────┘
                           │
                           ▼
              ┌────────────────────────┐
              │  calculateGrade()      │  对外入口
              │  - 信号处理注册         │
              │  - PCM 字节 → int       │
              │  - 非线性平方压缩       │
              └───────────┬────────────┘
                          │
                          ▼
              ┌────────────────────────┐
              │ innerCalculateGrade()  │  核心评分逻辑
              └───────────┬────────────┘
                          │
        ┌─────────────────┼─────────────────┐
        ▼                 ▼                 ▼
┌───────────────┐ ┌───────────────┐ ┌───────────────┐
│ 歌词时间中点   │ │ 非静默区间检测 │ │  情感数据准备  │
│ get_Lyric     │ │ get_notSilence│ │  get_dBFS     │
│ MiddleTimes() │ │ Range()       │ │               │
└───────┬───────┘ └───────┬───────┘ └───────┬───────┘
        │                 │                 │
        │                 ▼                 │
        │        ┌─────────────────┐        │
        │        │ 32 组参数穷举    │        │
        │        │ ┌─────────────┐ │        │
        │        │ │ 静默检测     │ │        │
        │        │ │ → 非静默区间 │ │        │
        │        │ │ → 中点时间   │ │        │
        │        │ │ → DTW 评分   │ │        │
        │        │ └─────────────┘ │        │
        │        │ 选 DTW 最小组合  │        │
        │        └────────┬────────┘        │
        │                 │                 │
        ▼                 ▼                 ▼
┌───────────────┐ ┌───────────────┐ ┌───────────────┐
│  get_rhythm   │ │  最优非静默    │ │  get_emotion  │
│  Grade()      │ │  区间          │ │  Grade()      │
│  DTW 对比     │ │                │ │  时长+音量    │
└───────┬───────┘ └───────┬───────┘ └───────┬───────┘
        │                 │                 │
        ▼                 ▼                 ▼
   节奏分            发声段中点         情感分
        │                 │                 │
        └─────────────────┴─────────────────┘
                          │
                          ▼
              ┌────────────────────────┐
              │  返回 [节奏分, 情感分]  │
              └────────────────────────┘
```

---

## 4. 数据结构

### `struct IndexRange`

```c
struct IndexRange {
    int length;    // data 数组的有效长度
    int *data;     // 成对存放 [start0, end0, start1, end1, ...]
};
```

- 用于表示一组区间。
- `length` 是 `data` 的元素个数，区间数 = `length / 2`。
- 单位可能是 **帧（frame）** 或 **毫秒（ms）**，取决于使用场景。

### 歌词时间数组 `lyricList`

```
lyricList = [t1_start, t1_dur, t2_start, t2_dur, t3_start, t3_dur, ...]
```

- 单位：毫秒（ms）。
- 每两个字一组：起始时间 + 持续时长。
- `lyricLength` = 数组长度 = 字数 × 2。

---

## 5. 模块详解

### 5.1 对外入口

#### `calculateGrade()`

```c
float *calculateGrade(unsigned char pcmList[], int pcmLength,
                      int lyricList[], int lyricLength);
```

**职责**：

1. 注册 `SIGFPE`、`SIGSEGV` 信号处理，防止崩溃。
2. 把 `unsigned char` 字节流转换成 `int` 数组。
3. 对每个样本做**非线性平方压缩**。
4. 调用 `innerCalculateGrade()` 得到分数。
5. 返回 `[节奏分, 情感分]`。

**PCM 转换细节**：

```c
// 字节合成（小端）
int value = ((int)((pcmList[2*i + 1] << 8) | pcmList[2*i]));
// 无符号 → 有符号
if (value > 32767) value -= 65536;
// 非线性压缩：归一化到 [-1,1]，乘 256，平方
int newValue = (int)(1.0 * value / 32768 * 256 * value / 32768 * 256);
```

**为什么平方**：

- 变成非负数，避免正负抵消。
- 压缩动态范围，放大低音量部分的相对差异。

#### `calculateGradeInt()`

```c
float *calculateGradeInt(int pcmList[], int pcmLength,
                         int lyricList[], int lyricLength);
```

直接接收 `int` PCM 数组，跳过字节转换和压缩，直接调用 `innerCalculateGrade()`。适合已经预处理好的场景。

---

### 5.2 PCM 预处理

| 步骤 | 操作 | 结果 |
|------|------|------|
| 1 | 每 2 字节拼成 16-bit 小端整数 | 0 ~ 65535 |
| 2 | 大于 32767 的减 65536 | -32768 ~ 32767 |
| 3 | 归一化除以 32768，乘 256，平方 | 0 ~ 65536 |

**注意**：第 3 步之后，数据不再是物理 PCM 幅度，而是"能量域"的单调指标。后续所有 RMS、dBFS 都是在这个域里算的。

---

### 5.3 歌词时间中点计算

#### `get_LyricMiddleTimes()`

```c
float* get_LyricMiddleTimes(int lyricTimes[], int lyricTimeLength);
```

**逻辑**：

```c
for (int index=0; index < lyricTimeLength -1; index++){
    lyricMiddleTimeArray[index] = lyricTimes[2*(index + 1)] 
                                + lyricTimes[2*(index + 1) + 1] / 2;
}
```

**解释**：

- 输入是 `[t0_start, t0_dur, t1_start, t1_dur, ...]`。
- 对第 `index` 个字，取的是**下一个字** `index+1` 的起始时间 + 时长的一半。
- 实际上相当于取每个字的**中点时刻**（有个 off-by-one 的偏移，见注意事项）。
- 最后一个字如果时长 > 1000ms，取 75% 位置而不是 50%。

**输出**：每个字的代表时刻数组，单位 ms。

---

### 5.4 非静默区间检测

#### `get_notSilenceRangeLogic()`

这是最核心的信号处理函数。

```c
struct IndexRange get_notSilenceRangeLogic(
    int lyricIndex,      // 仅日志用，不参与计算
    int audio[],         // PCM 数据
    int length,          // PCM 长度（帧）
    int silence_min,     // 最小静默时长（ms）
    float dbfs,          // 判定阈值（dBFS）
    int channels,        // 声道数 = 2
    int sr,              // 采样率 = 48000
    int bits             // 位深 = 16
);
```

**处理流程**：

```
① 参数换算
   reversThreshold  = reverse_dBFS(dbfs, bits)        // dBFS → 线性阈值
   seek_step        = 10ms × channels × sr            // 滑动步长
   minSilenceFrams  = silence_min × channels × sr     // 窗口长度

② 滑动窗口检测
   窗口长度 = minSilenceFrams，步长 = seek_step
   对每个窗口：
     取窗口内 PCM → 算 RMS → 与阈值比较
     RMS > 阈值 → 标记为"有声"（flag=1）
     否则       → 标记为"静默"（flag=0）

③ 收集静默窗口
   把所有 flag=0 的窗口起始帧号存入 silenceIndexList

④ 合并连续静默窗口为静默区间
   判断断裂条件：
     前一个静默窗口 + seek_step != 当前静默窗口  （不连续）
     且 当前静默窗口 > 前一个 + minSilenceFrams  （间隔足够大）
   合并得到 silenceIndexRange = [s0_start, s0_end, s1_start, ...]

⑤ 取补集得到非静默区间
   相邻两个静默段之间的空隙 = 非静默段
   过滤掉长度 < 20ms 的段

⑥ 返回非静默区间列表
```

**关键点**：

- 窗口**高度重叠**（窗长 110ms，步长 10ms），所以静默索引大量重复，需要靠连续性判断合并。
- `silenceFlagList[index] == 1` 表示**有声**，命名容易误解。
- 返回的是**非静默区间**，但函数名和中间变量都在讲"静默"。
- 最多处理 `MAX_WORDS_ONE_LINE`（19）个静默段。

---

### 5.5 静默检测参数搜索

#### `get_notSilenceRange()`

静默检测有两个关键参数，代码用**双重循环穷举 32 组组合**：

```c
int min_silence_ms[4] = {110, 90, 70, 50};
float dBFS_rate[8] = {0.9, 0.95, 1, 1.05, 1.1, 1.15, 1.2, 1.25};
```

对每组组合：

```
① 调用 get_notSilenceRangeLogic() 得到非静默区间
② 把区间端点从帧转成 ms
③ 取每个区间的中点时间
④ 用 DTW 比较"区间中点"和"歌词中点"的距离
⑤ 综合评分：
   rangeScore = 10000 × |区间数 - 歌词字数| + DTW距离
⑥ 保留 rangeScore 最小的组合
```

**为什么用 10000 倍惩罚**：

- 区间数和歌词字数不一致是严重问题，用大权重压制。
- DTW 距离通常只有几百到几千，10000 倍能确保数量差异优先被惩罚。

**输出**：最优参数下检测出的非静默区间。

---

### 5.6 DTW 动态时间规整

#### `dtw()`

```c
float dtw(float singerPcm[], int singerLength,
          float userPcm[], int userLength);
```

**算法步骤**：

1. **构造距离矩阵**：`D[i][j] = |singerPcm[i] - userPcm[j]|`
2. **初始化代价矩阵**：`cost[0][0] = 0`，第一行/列 = `MAX_DISTANCE`
3. **动态规划**：
   ```
   cost[i][j] = D[i-1][j-1] + min(cost[i-1][j], cost[i-1][j-1], cost[i][j-1])
   ```
4. **返回** `cost[m][n]`

**特点**：

- 允许时间轴弹性对齐，适合长度不同的序列。
- 只返回距离，不做路径回溯。
- 未做路径长度归一化，调用方需自行除以长度。

**在本项目中的两次使用**：

| 调用处 | 比较对象 | 目的 |
|--------|----------|------|
| `get_notSilenceRange` | 歌词中点 vs 发声段中点 | 选最优静默检测参数 |
| `get_rhythmGrade` | 歌词中点 vs 发声段中点 | 直接算节奏分 |

> 注意：比较的都是**时间序列**（ms），不是音频幅度。

---

### 5.7 节奏评分

#### `get_rhythmGrade()`

```c
float get_rhythmGrade(float lyricTimes[], int lyricTimesLength,
                      float splitTimes[], int splitTimesLength);
```

**公式**：

```c
minDistance  = dtw(歌词中点, 发声段中点)
meanDistance = minDistance / 歌词字数
grade = 1 - MAX(0, (meanDistance - 200) / 1000)
          - 0.1 * |歌词字数 - 发声段数|
```

**解释**：

| 项 | 含义 |
|----|------|
| `1` | 基础分 |
| `MAX(0, (meanDistance - 200)/1000)` | 平均偏差 200ms 内不扣分，超过后每 1000ms 扣 1 分 |
| `0.1 * |字数差|` | 发声段数和歌词字数每差一个扣 0.1 分 |

最后用 `LIMIT` 限制到 `[0, 1]`。

---

### 5.8 情感评分

#### `get_emotionGrade()`

```c
float get_emotionGrade(float splitTimes[], int splitRangeCount,
                       float dbfsArrays[], int dbfsLength);
```

由两部分加权（0.7 + 0.3）：

**① 发声时长得分（权重 0.7）**

```c
timeMin = 最短段时长
timeMax = 最长段时长
durationsGrade = 1 - timeMin / timeMax
```

- 各段时长差异越大，说明演唱有节奏起伏，得分越高。
- 如果所有段一样长，`timeMin/timeMax = 1`，得 0 分。

**② 音量幅度得分（权重 0.3）**

```c
dbfsMin = 最小 dBFS
dbfsMax = 最大 dBFS
dbfsGrade = |dbfsMax - dbfsMin| / 15
```

**三个异常判断**：

```c
if (dbfsMax < -80) dbfsGrade = 0;      // 几乎没声音
if (dbfsMin > -30) dbfsGrade = 0;      // 一直很响没起伏
if (|dbfsMax - dbfsMin| > 12) dbfsGrade = 0;  // 动态范围异常（可能爆音）
```

**综合**：

```c
returnGrade = 0.7 * durationsGrade + 0.3 * dbfsGrade
```

最后 `LIMIT` 到 `[0, 1]`。

---

## 6. 关键指标：RMS 与 dBFS

| 概念 | 定义 | 单位 | 范围 |
|------|------|------|------|
| **RMS** | `sqrt(Σx²/n)`，平均能量 | 线性 | 0 ~ 满量程 |
| **dBFS** | `20*log10(RMS/满量程)` | dB | -∞ ~ 0 |

**转换公式**：

```
dBFS = 20 * log10(RMS / FullScale)
RMS  = 10^(dBFS/20) * FullScale
```

**本代码中的实现**：

```c
// RMS（注意：漏了平方，见注意事项）
float get_rms(int pcmList[], int length){
    float sum = 0;
    for (int i = 0; i < length; i++) sum += pcmList[i];  // 应为 pcmList[i]²
    return sqrt(sum / length);
}

// dBFS
float get_dBFS(int pcmList[], int length, int bits){
    float rms = get_rms(pcmList, length);
    return 20 * log10(rms / pow(2, bits - 1));
}

// 逆运算
float reverse_dBFS(float dbfs, int bits){
    return pow(10, dbfs/20) * pow(2, bits - 1);
}
```

**在项目中的用途**：

| 场景 | 用到的量 | 方式 |
|------|----------|------|
| 静默检测 | dBFS → 线性 | 阈值转线性后与 RMS 比 |
| 情感评分 | dBFS | 取最大/最小算动态范围 |

---

## 7. 完整调用链

```
calculateGrade()                              ← 对外入口
├── 信号处理注册 (SIGFPE, SIGSEGV)
├── PCM 预处理
│   ├── 字节 → 16-bit 有符号
│   └── 非线性平方压缩
└── innerCalculateGrade()                     ← 核心逻辑
    ├── get_LyricMiddleTimes()                ← 歌词中点
    ├── get_notSilenceRange()                 ← 非静默区间（含参数搜索）
    │   └── 循环 32 组参数：
    │       ├── get_notSilenceRangeLogic()    ← 单次检测
    │       │   ├── get_rms()                 ← 窗口能量
    │       │   └── reverse_dBFS()            ← 阈值转换
    │       ├── get_frameIndexToTimeIndex()   ← 帧 → ms
    │       ├── get_splitMiddleTimes()        ← 区间中点
    │       └── dtw()                         ← 参数评分
    ├── get_frameIndexToTimeIndex()           ← 最优区间 → ms
    ├── get_splitMiddleTimes()                ← 最优区间中点
    ├── get_rhythmGrade()                     ← 节奏分
    │   └── dtw()
    └── get_emotionGrade()                    ← 情感分
        └── get_dBFS()                        ← 每段音量
            └── get_rms()
```

---

## 8. 参数说明

| 宏 | 值 | 说明 |
|----|-----|------|
| `MAX_WORDS_ONE_LINE` | 20 | 一行最多处理的字数（实际 19） |
| `CHANNELS` | 2 | 声道数 |
| `SAMPLE_RATE` | 48000 | 采样率（Hz） |
| `BITS` | 16 | 采样位深 |
| `MAX_DISTANCE` | 2147483647 | DTW 边界无穷大 |
| `MIN(X,Y)` | `(X)<(Y)?(X):(Y)` | 最小值宏 |
| `MAX(a,b)` | `(a)>(b)?(a):(b)` | 最大值宏 |
| `LIMIT(a)` | `MAX(0, MIN(1, a))` | 限制到 [0,1] |

**静默检测参数搜索空间**：

| 参数 | 取值 |
|------|------|
| `min_silence_ms` | 110, 90, 70, 50 |
| `dBFS_rate` | 0.9, 0.95, 1, 1.05, 1.1, 1.15, 1.2, 1.25 |
| 组合数 | 4 × 8 = 32 |

---

## 9. 已知问题与注意事项

### 算法层面

| 问题 | 说明 | 影响 |
|------|------|------|
| **`get_rms` 漏了平方** | 算的是 `sqrt(mean(x))` 而非 `sqrt(mean(x²))` | 对原始音频失效；因前级平方压缩而勉强可用 |
| **双重压缩** | PCM 预处理已平方，RMS 又没平方 | 数值不是标准能量，但单调性保留 |
| **`lyricIndex` 恒为 0** | 调用方硬编码传 0 | 仅日志用，无算法影响 |
| **DTW 未归一化** | 返回累积距离，长序列天然值大 | 调用方需自行除以长度 |
| **DTW 无窗口约束** | 允许任意弯曲 | 可能产生不合理对齐 |
| **`MAX_WORDS_ONE_LINE` 硬限制** | 静默段超过 19 段截断 | 可能丢失后续内容 |

### 实现层面

| 问题 | 说明 |
|------|------|
| **VLA 栈分配** | `float distanceMatric[m][n]`、`int silenceIndexList[n]` 等变长数组，长音频可能栈溢出 |
| **`&` 而非 `&&`** | 第 4 步断裂判断用位与，两个条件都求值（此处无副作用） |
| **内存泄漏** | `calculateGrade` 里 `free(intPcmList)` 被注释掉，`free(lRes)` 也被注释 |
| **`static` 缓冲区** | `getsubstr` 用 `static char buf[100]`，不可重入（该函数未被调用） |
| **命名反向** | `get_notSilenceRangeLogic` 内部先找静默再取补集；`silenceFlagList` 的 1 表示有声 |
| **`testCharData` / `testStringData`** | 占位函数，无实际逻辑 |

### 平台相关

- 使用 `__android_log_print` 输出日志，移植到其他平台需替换 `LOGD` 宏。
- 信号处理 `signal(SIGFPE, ...)` 在部分平台行为不一致。

---

## 10. 移植到 Python 的建议

### 推荐库

| 需求 | 库 | 用法 |
|------|-----|------|
| 加载音频 | `librosa` | `y, sr = librosa.load("audio.wav")` |
| 计算 RMS | `librosa.feature.rms` | `rms = librosa.feature.rms(y=y)[0]` |
| 计算 dBFS | `numpy` | `20*np.log10(rms/满量程)` |
| DTW | `dtw-python` / `fastdtw` | `distance, path = fastdtw(x, y)` |
| 数组运算 | `numpy` | 向量化替代循环 |

### 标准 RMS 实现

```python
import numpy as np

def calculate_rms(signal):
    """标准 RMS：sqrt(mean(x²))"""
    return np.sqrt(np.mean(signal ** 2))

def calculate_dbfs(signal, full_scale=32768):
    """标准 dBFS"""
    rms = calculate_rms(signal)
    if rms == 0:
        return float('-inf')
    return 20 * np.log10(rms / full_scale)
```

### 简化版 Python 流程

```python
import numpy as np
import librosa
from fastdtw import fastdtw

def calculate_grade(pcm_bytes, lyric_list):
    # 1. PCM 预处理
    audio = np.frombuffer(pcm_bytes, dtype=np.int16).astype(np.float32)
    audio = (audio / 32768 * 256) ** 2   # 平方压缩

    # 2. 歌词中点
    lyric_mid = np.array([
        lyric_list[2*(i+1)] + lyric_list[2*(i+1)+1] / 2
        for i in range(len(lyric_list)//2 - 1)
    ])

    # 3. 静默检测 + 参数搜索
    best_ranges = search_best_silence(audio, lyric_mid)

    # 4. 节奏分
    split_mid = [(s + e) / 2 for s, e in best_ranges]
    rhythm_dist, _ = fastdtw(lyric_mid, split_mid)
    mean_dist = rhythm_dist / len(lyric_mid)
    rhythm_grade = 1 - max(0, (mean_dist - 200) / 1000) \
                     - 0.1 * abs(len(lyric_mid) - len(split_mid))
    rhythm_grade = np.clip(rhythm_grade, 0, 1)

    # 5. 情感分
    durations = [e - s for s, e in best_ranges]
    dur_grade = 1 - min(durations) / max(durations)

    dbfs_list = [calculate_dbfs(audio[s:e]) for s, e in best_ranges]
    dbfs_grade = abs(max(dbfs_list) - min(dbfs_list)) / 15
    if max(dbfs_list) < -80 or min(dbfs_list) > -30 \
       or abs(max(dbfs_list) - min(dbfs_list)) > 12:
        dbfs_grade = 0

    emotion_grade = np.clip(0.7 * dur_grade + 0.3 * dbfs_grade, 0, 1)

    return [rhythm_grade, emotion_grade]
```

---

## 附录：函数速查表

| 函数 | 职责 | 输入 | 输出 |
|------|------|------|------|
| `calculateGrade` | 对外入口 | PCM 字节流、歌词 | `[节奏分, 情感分]` |
| `calculateGradeInt` | int 版入口 | int PCM、歌词 | `[节奏分, 情感分]` |
| `innerCalculateGrade` | 核心评分 | int PCM、歌词 | `[节奏分, 情感分]` |
| `get_LyricMiddleTimes` | 歌词中点 | 歌词时间数组 | 中点数组（ms） |
| `get_notSilenceRange` | 参数搜索 | PCM、歌词中点 | 最优非静默区间 |
| `get_notSilenceRangeLogic` | 单次静默检测 | PCM、参数 | 非静默区间 |
| `get_rms` | 窗口能量 | PCM 窗口 | RMS 值 |
| `get_dBFS` | 转分贝 | PCM 窗口 | dBFS |
| `reverse_dBFS` | dBFS 转线性 | dBFS | 线性幅度 |
| `dtw` | 序列对齐距离 | 两个序列 | 累积距离 |
| `get_rhythmGrade` | 节奏评分 | 歌词中点、发声中点 | 节奏分 |
| `get_emotionGrade` | 情感评分 | 区间时长、dBFS | 情感分 |
| `get_frameIndexToTimeIndex` | 帧转 ms | 帧索引 | ms 时间 |
| `get_splitMiddleTimes` | 区间中点 | 区间时间 | 中点数组 |

---
