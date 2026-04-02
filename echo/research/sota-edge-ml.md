# SOTA Edge ML Research: ESP32-C3 On-Device Inference

**Target hardware:** ESP32-C3 (160 MHz RISC-V RV32IMC, 400 KB SRAM, 4 MB flash, no PSRAM)  
**Budget:** 813K parameters for "Kernel Whisper" on-device AI  
**Research date:** 2026-04-02  

---

## 1. TinyML Frameworks for ESP32-C3

### 1.1 TensorFlow Lite Micro (esp-tflite-micro)

- **Repo:** [espressif/esp-tflite-micro](https://github.com/espressif/esp-tflite-micro) (622 stars, actively maintained as of April 2026)
- **Status:** Officially supported on ESP32-C3 via ESP-IDF v4.4+. Espressif maintains vendor-specific optimizations through ESP-NN integration.
- **Available examples:** hello_world, micro_speech, person_detection
- **ESP-NN speedup:** On ESP32 (Xtensa), person_detection drops from 4084ms to 380ms with ESP-NN. ESP32-C3 RISC-V has less aggressive SIMD optimization than S3/P4, so expect less dramatic speedups, but the framework compiles and runs.
- **Verdict:** Production-ready for ESP32-C3. Best ecosystem, most models, most community support.

### 1.2 TinyMaix (Sipeed)

- **Repo:** [sipeed/TinyMaix](https://github.com/sipeed/TinyMaix) (1044 stars)
- **Core:** ~400 lines of C, binary <3 KB. Supports INT8/FP32/FP16, experimental FP8.
- **RISC-V support:** RV32P acceleration (packed SIMD). ESP32-C3 uses base RV32IMC without packed extensions, so it runs in generic mode.
- **ESP32-C3 benchmarks (from TinyMaix benchmark suite, 160 MHz, model in flash):**

| Model | Input | Flash | RAM | Inference Time |
|-------|-------|-------|-----|----------------|
| MNIST | 28x28x1 | 2.4 KB | 1.4 KB | **6 ms** |
| CIFAR-10 | 32x32x3 | 89 KB | 11 KB | **127 ms** |
| VWW96 (MobileNet v1 0.25) | 96x96x3 | 227 KB | 54 KB | **1430 ms** |
| MobileNet v1 0.25 128x128 | 128x128x3 | 485 KB | 96 KB | **2370 ms** |

- **Key insight:** ESP32-C3 is ~2x slower than ESP32-S3 per-clock due to lack of Xtensa SIMD and single-issue RV32 pipeline. The normalized efficiency at 100 MHz is 203ms for CIFAR vs 206ms for ESP32-S3 -- nearly identical raw CPU efficiency; the gap is purely clock speed.
- **Verdict:** Excellent for ultra-small models (MNIST-class). VWW96 at 1.4s is borderline usable for non-interactive classification. MobileNet at 2.4s is too slow for real-time but works for batch/triggered inference.

### 1.3 ESP-DL (Espressif)

- **Repo:** [espressif/esp-dl](https://github.com/espressif/esp-dl) (963 stars)
- **Status:** Supports ESP32-C3 technically, but primary optimization targets are ESP32-S3 and ESP32-P4 (which have vector extensions). ESP32-C3 operator implementations fall back to C -- significantly slower than optimized paths.
- **ONNX alignment:** Operator interface aligned with ONNX opset 13. Supports 31 operators.
- **Model Zoo:** Face detection, face recognition, cat detection -- but these require more RAM/compute than C3 can provide.
- **Verdict:** Use for ONNX-compatible model deployment if you need specific operators. Not the best choice for ESP32-C3 specifically due to lack of hardware acceleration.

### 1.4 ONNX-to-C Compilers

- **onnx2c** ([kraiskil/onnx2c](https://github.com/kraiskil/onnx2c)): Converts ONNX models directly to C code. Zero dynamic allocation, no library dependencies beyond math.h. Ideal for ESP32-C3 where every KB matters.
- **cONNXr** ([alrevuelta/cONNXr](https://github.com/alrevuelta/cONNXr)): Pure C99 ONNX runtime, zero dependencies. Good for embedded but heavier than onnx2c.
- **Verdict:** onnx2c is the best option for squeezing ONNX models onto ESP32-C3 with minimal overhead.

### 1.5 Other Notable Frameworks

- **NNoM:** Lightweight NN framework for MCUs, similar niche to TinyMaix
- **Edge Impulse:** Cloud-based training platform that exports to TFLite Micro. Good workflow for ESP32.
- **MCUNet/TinyEngine (MIT):** Research framework achieving SOTA accuracy under 256 KB SRAM. Achieves 71-77% on 50-class ImageNet under 256 KB SRAM / 1 MB flash. Impressive but primarily research code.

---

## 2. What Models Can Run on 400 KB SRAM / 4 MB Flash?

### 2.1 Keyword Spotting / Wake Word Detection

**This is the sweet spot for ESP32-C3.**

- **Espressif WakeNet9s:** Purpose-built for ESP32-C3 (no PSRAM, no SIMD). Depthwise separable convolution architecture. Supports up to 5 wake words. Uses MFCC features with 30ms window/step at 16 KHz.
- **Typical KWS model:** ~18-22 KB flash for INT8 quantized 1D CNN on MFCC features (62 frames x 13 coefficients). Entire inference pipeline fits in <50 KB SRAM.
- **Edge Impulse KWS:** Custom wake words with INT8 quantization, <2% accuracy loss vs FP32.
- **Inference latency:** 50-60 ms typical for keyword spotting.
- **RAM budget:** ~20-50 KB SRAM for model + activations.

**Assessment for 813K parameters:** A keyword spotting model with 813K parameters at INT8 = ~813 KB. That fits in flash but is actually overkill for KWS -- typical KWS models use 20-100K parameters. You could run a very capable multi-keyword system.

### 2.2 Anomaly Detection (Battery/Power Monitoring)

- **Autoencoder approach:** Tiny autoencoders for time-series anomaly detection. Models as small as 1-5 KB flash.
- **Proven on ESP32:** Implementations achieving F1=0.87 on anomaly detection with on-device training times of ~12 minutes, inference latency <9 ms.
- **RAM:** <10 KB for small autoencoders processing sensor readings.
- **Power:** Single-digit milliwatt inference budgets.

**Assessment:** Perfectly feasible. A 813K-parameter anomaly detector is massive overkill -- you could run a capable anomaly detector in <50K parameters and still have budget for other models.

### 2.3 Simple Text/Sequence Classification

- **Feasible with embeddings in flash.** Small 1D CNNs or tiny RNNs for classifying short text/sequences.
- **Practical model size:** 50-200 KB flash, 10-30 KB SRAM.
- **Use case for ECHO:** Classify incoming API payloads, categorize notification types, or pattern-match button sequences.

### 2.4 Image Classification

- **VWW (Visual Wake Words):** 227 KB flash, 54 KB RAM, 1.4s inference on ESP32-C3 via TinyMaix. Binary classification (person present / not present). MCUNetV2 achieves >90% accuracy under 32 KB SRAM for VWW.
- **CIFAR-10 class (10 categories):** 89 KB flash, 11 KB RAM, 127 ms inference. Very usable.
- **MobileNet v1 0.25 (1000 class):** 485 KB flash, 96 KB RAM, 2.4s inference. Possible but slow.
- **ESP32-C3 does NOT have a camera interface**, so image classification would mean analyzing display buffer contents or pre-captured data.

**Assessment:** CIFAR-class models (small images, few classes) run well. For e-paper display content analysis, you would need to downsample significantly. 32x32 grayscale classification at 127ms is practical.

### 2.5 Gesture Recognition from Button Patterns

- **Tiny 1D CNN or LSTM on button timing sequences.** Model size: 5-20 KB.
- **Inference:** <10 ms.
- **Training:** Can be done on-device with incremental learning (see section 5).

**Assessment:** Trivially feasible. This barely dents the 813K parameter budget.

---

## 3. Speech Recognition on ESP32-C3

### 3.1 Whisper on ESP32-C3: Not Possible

- **whisper.cpp tiny model:** Requires ~390 MB RAM minimum (FP16) or ~77 MB (INT4 aggressive quantization). This is 200,000x more than ESP32-C3 has.
- **Even whisper.cpp on Raspberry Pi 4 (4 GB RAM)** takes several seconds per inference.
- **Verdict:** Full Whisper is completely impossible on ESP32-C3. Not even close. Not with any quantization scheme.

### 3.2 What IS Possible: Keyword Spotting

- **WakeNet9s (Espressif):** Runs on ESP32-C3 without PSRAM. Recognizes pre-trained wake words. This is the production-grade solution.
- **Google micro_speech example:** Available in esp-tflite-micro. Recognizes "yes"/"no"/"up"/"down" with a tiny depthwise separable CNN. ~18 KB model.
- **Edge Impulse custom KWS:** Train custom 5-10 word vocabulary, deploy as INT8 TFLite model. Offline, no cloud needed.

### 3.3 Practical Architecture for ECHO

Instead of on-device ASR, the viable pattern is:

```
[Mic/Audio Input] --> [On-device KWS: wake word detection]
                          |
                          v (on wake word trigger)
                   [Stream audio via WiFi to cloud/server]
                          |
                          v
                   [Server-side Whisper/ASR]
                          |
                          v
                   [Return transcription to ESP32-C3]
```

This is how every ESP32 voice assistant project works in practice (2025-2026).

### 3.4 "Kernel Whisper" Reframing

Given the 813K parameter budget at INT8 = ~813 KB flash:

| Approach | Parameters | Flash | RAM | Latency |
|----------|-----------|-------|-----|---------|
| WakeNet9s (5 words) | ~100K est. | ~100 KB | ~30 KB | ~50 ms |
| Custom KWS (10 commands) | ~50-200K | ~50-200 KB | ~20-50 KB | ~50-60 ms |
| Small classifier cascade | ~200-400K | ~200-400 KB | ~30-60 KB | ~100-200 ms |
| **Combined pipeline** | **~500-800K** | **~500-800 KB** | **~60-100 KB** | **~200-300 ms** |

The name "Kernel Whisper" could mean: a local keyword/command recognizer (the "whisper" the device can hear) that gates more expensive cloud operations. 813K parameters is enough for a sophisticated multi-stage local audio pipeline.

---

## 4. Quantization for Extreme Constraints

### 4.1 INT8 Quantization (Standard for ESP32-C3)

- **4x size reduction** from FP32, typically <2% accuracy loss.
- **Supported by:** TFLite Micro, TinyMaix, ESP-DL, Edge Impulse.
- **ESP32-C3 has no FPU**, so INT8 inference is actually faster than FP32 on this chip.
- **Post-training quantization (PTQ):** Easy, supported by all frameworks. Slight accuracy loss.
- **Quantization-aware training (QAT):** Better accuracy, requires retraining. TensorFlow/Keras support this natively.

### 4.2 Sub-Byte Quantization (2-bit / Ternary)

- **xTern (ETH Zurich):** RISC-V ISA extension for ternary neural networks. Achieves 67% higher throughput than 2-bit equivalents, only 5.2% more power, 57% better energy efficiency. However, requires custom RISC-V extensions not present in ESP32-C3.
- **PULP-NN:** Optimized library for INT1/INT2/INT4 on RISC-V with packed-SIMD. Again requires PULP extensions.
- **XpulpNN:** INT4 and INT2 via packed-SIMD ISA extensions on RISC-V.

**ESP32-C3 reality check:** The C3 uses a basic RV32IMC core without packed-SIMD or custom extensions. Sub-byte quantization would need to be done in software with bit-packing, losing much of the theoretical speedup.

### 4.3 Binary Neural Networks (BNNs)

- **1-bit weights:** XNOR operations instead of multiply-accumulate. Theoretically 32x speedup on 32-bit hardware via bitwise ops.
- **Practical on RV32IMC:** Binary convolutions can be implemented as XNOR + popcount. ESP32-C3 lacks a hardware popcount instruction, so this is simulated via shifts (~4-8 instructions per popcount).
- **Accuracy cost:** Significant (10-30% drop on ImageNet-class tasks), but acceptable for binary classification tasks.

### 4.4 Practical Recommendation for 813K Parameters

| Quantization | Model Size (813K params) | Inference Speed | Accuracy Impact |
|-------------|-------------------------|-----------------|-----------------|
| FP32 | 3.25 MB (won't fit flash) | Slowest | Baseline |
| FP16 | 1.63 MB (fits flash) | Slow (no FPU) | ~0.5% loss |
| **INT8** | **813 KB (fits flash)** | **Fastest practical** | **1-2% loss** |
| INT4 | 407 KB | Fast (software decode) | 3-5% loss |
| INT2/Ternary | 203 KB | Medium (bit-packing overhead) | 5-15% loss |
| Binary (1-bit) | 102 KB | Fast (XNOR) | 10-30% loss |

**Recommendation:** INT8 is the practical sweet spot for ESP32-C3. INT4 is worth exploring if you need to fit more model into flash. Sub-byte quantization on ESP32-C3 without hardware support has diminishing returns.

---

## 5. On-Device Learning / Federated Learning

### 5.1 On-Device Training on ESP32

- **Proven feasible:** Research (TinyFL, 2023) demonstrated full federated learning cycle on ESP32 -- local training, aggregation, redistribution.
- **ESP32-CAM results (2025):** Autoencoder training on-device, F1=0.87, training time ~12 minutes for MNIST-class tasks.
- **TinyFed framework:** Local training accuracies up to 99.47% on ESP32 for sensor anomaly detection (temperature, humidity, voltage).

### 5.2 What's Realistic on ESP32-C3

- **Forward pass:** 400 KB SRAM is enough for inference on models up to ~100 KB activation memory.
- **Backward pass:** Requires ~2-3x the memory of forward pass for gradients. This limits on-device training to very small models (<50K parameters with INT8 gradients).
- **Practical approach:** Train a small last-layer classifier on-device while keeping the backbone frozen. This is transfer learning with a frozen feature extractor.

### 5.3 Incremental Learning from Button Patterns

- **Feasible:** A small gesture/pattern recognizer can be updated on-device.
- **Implementation:** Store a few hundred labeled examples in flash. Retrain a small FC layer periodically.
- **Memory:** <10 KB SRAM for a 2-layer FC classifier with 100 features.

### 5.4 Federated Learning Architecture for ECHO Fleet

```
[ECHO Device 1] --WiFi--> [Server aggregates models]
[ECHO Device 2] --WiFi-->        |
[ECHO Device N] --WiFi-->        v
                          [Updated global model]
                                 |
                                 v (OTA or next sync)
                          [Push to all devices]
```

This is viable but adds significant complexity. More practical for ECHO: periodic model updates via firmware OTA, informed by aggregate usage analytics from the fleet.

---

## 6. Feasibility Matrix for ECHO (813K Parameter Budget)

| Capability | Feasible? | Parameters Needed | Flash | RAM | Latency | Notes |
|-----------|-----------|------------------|-------|-----|---------|-------|
| Wake word (5 words) | YES | ~100K | ~100 KB | ~30 KB | ~50 ms | WakeNet9s, production-ready |
| Command recognition (10-20 words) | YES | ~200K | ~200 KB | ~40 KB | ~60 ms | Edge Impulse or custom KWS |
| Anomaly detection (battery/power) | YES | ~10-50K | ~10-50 KB | ~5-10 KB | <10 ms | Tiny autoencoder |
| Button gesture recognition | YES | ~5-20K | ~5-20 KB | ~2-5 KB | <5 ms | 1D CNN on timing sequences |
| Display content classification | YES | ~50-200K | ~50-200 KB | ~15-30 KB | ~130 ms | CIFAR-class, downsampled |
| Simple text classification | YES | ~50-100K | ~50-100 KB | ~10-20 KB | ~20-50 ms | 1D CNN on embeddings |
| Full ASR / Whisper | NO | >100M | >77 MB | >77 MB | N/A | 200,000x over budget |
| Image generation | NO | >1M | >10 MB | >1 MB | N/A | Not feasible |
| LLM inference | NO | >100M | >100 MB | >100 MB | N/A | Not feasible |

### Recommended Multi-Model Stack (fits within 813K budget)

| Component | Parameters | Purpose |
|-----------|-----------|---------|
| WakeNet9s | ~100K | Wake word trigger |
| Command classifier | ~150K | 15-20 voice commands |
| Anomaly detector | ~30K | Battery/power health |
| Gesture recognizer | ~10K | Button pattern learning |
| Content classifier | ~150K | Display state categorization |
| Scheduling predictor | ~50K | Usage pattern prediction |
| **Total** | **~490K** | **Leaves ~323K headroom** |

---

## 7. Key Takeaways

1. **ESP32-C3 is a capable TinyML platform** with real production frameworks (TFLite Micro, TinyMaix, WakeNet9s). It is not a toy.

2. **813K parameters at INT8 = 813 KB flash.** This is generous for ESP32-C3 TinyML. You can run a multi-model stack, not just one model.

3. **"Kernel Whisper" should be a keyword spotting + command recognition pipeline**, not actual speech-to-text. Real ASR requires 200,000x more memory than available.

4. **INT8 is the practical quantization sweet spot.** ESP32-C3 lacks the ISA extensions needed to benefit from sub-byte quantization.

5. **On-device learning is possible** for small models (gesture recognition, anomaly detection). Full federated learning is proven but adds complexity.

6. **TinyMaix benchmarks show ESP32-C3 at par with ESP32-S3 in per-clock efficiency** for neural network inference. The performance gap is purely clock speed (160 vs 240 MHz).

7. **The RISC-V ecosystem for ML is maturing fast** (xTern, PULP-NN, XpulpNN) but the ESP32-C3's base RV32IMC doesn't benefit from these extensions yet. Future ESP32-C-series chips with vector extensions will change this dramatically.

---

## Sources

### Frameworks
- [espressif/esp-tflite-micro](https://github.com/espressif/esp-tflite-micro) -- TFLite Micro for Espressif
- [sipeed/TinyMaix](https://github.com/sipeed/TinyMaix) -- Ultra-lightweight NN inference (~400 lines C)
- [espressif/esp-dl](https://github.com/espressif/esp-dl) -- Espressif deep learning library
- [espressif/esp-sr](https://github.com/espressif/esp-sr) -- Espressif speech recognition (WakeNet/MultiNet)
- [kraiskil/onnx2c](https://github.com/kraiskil/onnx2c) -- ONNX to C compiler for MCUs
- [alrevuelta/cONNXr](https://github.com/alrevuelta/cONNXr) -- Pure C ONNX runtime

### Research Papers and Articles
- [xTern: Energy-Efficient Ternary Neural Network Inference on RISC-V](https://arxiv.org/html/2405.19065v1)
- [PULP-NN: Accelerating Quantized Neural Networks on RISC-V](https://royalsocietypublishing.org/doi/10.1098/rsta.2019.0155)
- [MCUNet: Tiny Deep Learning on IoT Devices (MIT)](https://tinyml.mit.edu/mcunet/)
- [On-Device Training Under 256KB Memory](https://arxiv.org/pdf/2206.15472)
- [Tiny Machine Learning: Progress and Futures](https://arxiv.org/html/2403.19076)
- [Quantized Neural Networks for Microcontrollers: Comprehensive Review](https://arxiv.org/html/2508.15008v2)
- [Implementing Neural Networks on 10-cent RISC-V MCU](https://cpldcpu.com/2024/04/24/implementing-neural-networks-on-the-10-cent-risc-v-mcu-without-multiplier/)
- [Federated Learning for IoT: Enhancing TinyML with On-Board Training](https://www.sciencedirect.com/science/article/pii/S1566253523005055)

### Tutorials and Guides
- [TinyML with ESP32 Tutorial](https://www.teachmemicro.com/tinyml-with-esp32-tutorial/)
- [ESP32 Offline Voice Recognition Using Edge Impulse](https://circuitdigest.com/microcontrollers-projects/esp32-offline-voice-recognition-using-edge-impulse)
- [ESP-DL Getting Started](https://docs.espressif.com/projects/esp-dl/en/latest/getting_started/readme.html)
- [WakeNet Wake Word Model Documentation](https://docs.espressif.com/projects/esp-sr/en/latest/esp32/wake_word_engine/README.html)
- [TinyMaix Benchmark Suite](https://github.com/sipeed/TinyMaix/blob/main/benchmark.md)
- [Ultra-Low-Power MCUs in 2026: AI-Enabled Microcontrollers](https://promwad.com/news/ultra-low-power-mcus-in-2026-ai-tinyml)
