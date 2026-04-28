# FFT 信号分析模块开发计划书

## 项目目标

基于 STM32H743 + CMSIS DSP，实现对正弦波、三角波、方波的自动识别，并精确测量信号频率与峰峰值。

---

## 误差指标

| 指标 | 目标值 |
|------|--------|
| 频率误差 | 0.1% + 0.5Hz |
| 峰峰值误差 | 1% + 1mV |

---

## 第一阶段：基础框架（已完成）

| 函数 | 状态 | 说明 |
|------|------|------|
| `window()` | ✅ | Hanning 窗系数预计算，存入 `Window_OutputBuffer` |
| `FFT_Process()` | ✅ | 清零 → 去直流(待加) → 加窗 → CFFT → 幅度谱 → 归一化 |
| `Process_FFT_mag()` | ✅ | 找最大值，计算 `FFT_Freq`、`FFT_Ampl`、`Ud` |
| `Find_BaseIndex()` | ✅ | 将 `FFT_mag_max_index` 赋值给 `BaseIdx` |
| `wave_type_detect()` | ✅ | 3次谐波/基波比值判断波形类型 |

调用顺序：
```
window()          // 初始化时调用一次
    ↓
FFT_Process()     // 每帧新数据调用
    ↓
Find_BaseIndex()
    ↓
wave_type_detect()
    ↓
Calc_VPP()        // 第二阶段加入
```

---

## 第二阶段：精度优化（待开发）

### 2.1 去直流（高优先级）

在 `FFT_Process()` 填充 `FFT_Input` 前加均值去除，防止DC分量压制低频bin导致波形判断错误。

```c
float mean = 0;
for (int i = 0; i < ADC_LEN; i++) mean += ADC_Buffer[i];
mean /= ADC_LEN;
// 填充时：(ADC_Buffer[i] - mean) * Window_OutputBuffer[i]
```

### 2.2 自适应采样率

根据上一帧测得的频率自动切换 ADC 采样率，保证 FFT 频率分辨率和每周期点数均满足要求。

| 频率范围 | 推荐 fs | Δf | 每周期点数(基波) |
|---------|---------|-----|----------------|
| 100Hz ~ 1kHz | 20kHz | 19.5Hz | ≥200点 |
| 1kHz ~ 10kHz | 200kHz | 195Hz | ≥20点 |
| 10kHz ~ 100kHz | 2MHz | 1953Hz | ≥20点 |

### 2.3 抛物线插值频率估计

在峰值 bin 附近用三点插值，精度提升约 10~20 倍：

$$f_{true} = \left(k + \frac{X[k+1] - X[k-1]}{2X[k] - X[k-1] - X[k+1]}\right) \times \frac{f_s}{N}$$

### 2.4 按波形类型计算 VPP

| 波形 | 方法 | 说明 |
|------|------|------|
| 正弦波 | `VPP = FFT_Ampl * 2 / 0.5f` | FFT幅度需除以Hanning窗增益0.5 |
| 方波 | 时域 max-min | 平台多点，误差极小 |
| 三角波 | 时域 max-min + 抛物线峰值拟合 | 裸max-min误差可达80mV |

---

## 第三阶段：鲁棒性（待开发）

### 3.1 越界保护

若基波频率过高，3次谐波下标超出前半段频谱，默认判为正弦波：

```c
if (3 * BaseIdx >= FFT_LEN / 2) { wave_type = 1; return; }
```

### 3.2 分类迟滞

连续3帧结果一致才更新 `wave_type`，防止 FFT 抖动导致分类乱跳：

```c
static int vote[3] = {0};
static int vote_idx = 0;
vote[vote_idx % 3] = new_type;
vote_idx++;
if (vote[0] == vote[1] && vote[1] == vote[2])
    wave_type = vote[0];
```

### 3.3 方波去毛刺

用分位数代替绝对 max-min，避免偶发噪声点拉大 VPP。

---

## 调试验证步骤

1. **第一步**：接信号发生器，固定频率（1kHz），分别输入三种波形，串口打印 `ratio` 实测值，标定 `wave_type_detect()` 阈值
2. **第二步**：固定波形（正弦），改变频率（100Hz→100kHz），验证 `FFT_Freq` 误差
3. **第三步**：固定频率，改变幅度（0.5V→3V），验证 `VPP` 误差
4. **第四步**：加入第二、三阶段优化，对比优化前后误差

---

## 已知风险

| 风险 | 影响 | 对策 |
|------|------|------|
| 未去直流 | 低频段波形判断错误 | 第二阶段加均值去除 |
| 频率抖动导致分类跳变 | wave_type 不稳定 | 加迟滞投票 |
| 100kHz时3次谐波接近奈奎斯特 | 谐波幅值失真 | 越界保护+提高fs |
| Hanning窗增益未校正 | 正弦VPP偏小50% | VPP计算时除以0.5 |
