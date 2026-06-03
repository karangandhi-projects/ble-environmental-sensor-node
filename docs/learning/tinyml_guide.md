# TinyML on the ESP32-C3: A Developer's Guide from First Principles

**Project:** BLE Environmental Sensor Node — ESP32-C3 / ESP-IDF v5.2.3 / NimBLE
**Audience:** C and embedded systems developer who has never done machine learning
**Goal:** Understand every line of `tinyml_inference.c`, why the model was trained the way it was, and how inference fits into the BLE data pipeline

---

## Table of Contents

1. What is Machine Learning?
2. Classification vs Regression
3. Neural Networks from First Principles
4. Training: Making the Network Learn
5. TinyML: ML on Microcontrollers
6. This Project's Model in Detail
7. Anomaly Detection
8. End-to-End Flow in This Project

---

## 1. What is Machine Learning?

### The Core Idea

Imagine you are writing firmware to decide whether a room is "comfortable". Your first instinct is to write if-else rules:

```c
if (temp > 18 && temp < 25 && humidity < 60) {
    return COMFORTABLE;
}
```

This works for clean cases. But real environments are messy. What happens at 25.3°C with 61% humidity and a pressure dip? Is that still comfortable? What if the sensor has ±2°C of measurement noise? What if your rule for "warm" and your rule for "comfortable" overlap at exactly 25°C?

Machine learning solves this differently. Instead of hand-coding the decision boundary, you collect examples of known outcomes and let the computer learn the boundary from the data.

**The core idea in one sentence:** ML finds a function from inputs to outputs by adjusting internal parameters until the function fits the observed data.

### The Vocabulary

- **Features (inputs):** The measurable quantities you feed into the model. In this project: `temp_c`, `humidity_pct`, `pressure_hpa` — three floating-point numbers.
- **Labels (outputs):** The correct answer for each training example. Here: one of `comfortable`, `warm`, `cold`, `humid`, `danger`.
- **Training:** The process of showing the model many (feature, label) pairs and adjusting its internal numbers until it makes correct predictions.
- **Inference:** Using the trained model on new, unseen inputs to get a prediction. This is all the firmware ever does.
- **Model:** The trained function — in our case, a small neural network with 245 numerical parameters stored in `ml_weights.h`.

### Why Not Just Use if-else?

Three reasons matter for this project:

1. **Class boundaries overlap.** A temperature of 25°C with 70% humidity sits between `comfortable` and `humid`. No hard threshold is correct — the right answer depends on the combination of all three features simultaneously. An if-else chain can only check one condition at a time; a neural network computes over all features at once.

2. **Sensor noise.** The simulated sensor adds up to ±2°C, ±2% humidity, and ±2 hPa of drift on each reading (applied in `sensor_provider.c`). A hand-coded rule at exactly 25°C would flip between classes every sample. The trained model has seen this variation in training data and learns stable, smooth boundaries.

3. **Real-world variation.** The training data combines 1500 synthetic samples with real sensor logs. A model trained on that distribution handles corner cases that no developer would think to write a rule for.

**Key takeaway:** Use ML when the decision depends on the combination of inputs in ways that are hard to enumerate by hand, or when the data has noise that would cause brittle rules to flip.

---

## 2. Classification vs Regression

### Two Kinds of Prediction Problems

**Regression** outputs a continuous number. "What will the temperature be in one hour?" is a regression problem — the answer is something like 23.7°C.

**Classification** outputs a discrete category. "What kind of environment is this?" is a classification problem — the answer is one of a fixed set of labels. This project is a 5-class classification problem:

| Class ID | Label | Meaning |
|----------|-------|---------|
| 0 | comfortable | Temperature and humidity within human comfort range |
| 1 | warm | High temperature |
| 2 | cold | Low temperature |
| 3 | humid | High humidity even at normal temperature |
| 4 | danger | Extreme temperature (risk of hardware damage or health hazard) |
| 5 | anomaly | The classifier cannot confidently assign any class (not a trained class — see Section 7) |

### Why Five Mutually-Exclusive Classes (and why softmax, not per-class sigmoids)?

A reading is exactly one of {comfortable, warm, cold, humid, danger} — these are **mutually exclusive**. That single fact drives two design choices:

- **Why softmax on the output, not five independent sigmoids?** A sigmoid-per-class head answers "is it warm? is it humid?" *independently*, allowing a reading to be 90% warm *and* 90% humid at once (multi-label). That's wrong here: a room is one category at a time. Softmax couples the outputs — they compete and sum to 1 — which is the correct prior for a "pick exactly one" problem. It also gives the anomaly trick (§7) its meaning: "no class above 50%" only makes sense when the classes share a probability budget.
- **Why these five labels and not more/fewer?** They map to the distinct *actions* a downstream app cares about: nominal (comfortable), too hot (warm), too cold (cold), too damp (humid), and a hardware/health hazard (danger). Splitting finer (e.g. "slightly warm" vs "hot") would demand sharper boundaries than noisy ±2°C sensors can support; merging coarser would lose the danger signal. Five is the granularity the data can actually distinguish — confirmed by the 98.83% test accuracy holding up.

### Softmax: Turning Raw Scores Into Probabilities

A neural network's final layer produces raw numbers called **logits** — one per class. These numbers are unbounded and do not directly mean anything. For our 5 classes, the network might output something like `[2.1, -0.3, -1.8, 0.4, -2.5]`.

Softmax converts these raw scores into a proper probability distribution — all values between 0 and 1, and they sum to exactly 1:

```
softmax(z_i) = exp(z_i) / sum(exp(z_j) for all j)
```

The exponential function amplifies differences. A small advantage in the raw score becomes a large advantage in probability. After softmax, our example might become: `[0.82, 0.07, 0.02, 0.09, 0.00]` — telling you the model is 82% confident this is "comfortable".

The firmware implements this in `tinyml_inference.c` with a numerical stability trick: subtract the maximum value before exponentiating, which prevents floating-point overflow without changing the result.

```c
/* Softmax — numerically stable implementation */
float max_val = out[0];
for (int i = 1; i < ML_OUTPUT_SIZE; i++) {
    if (out[i] > max_val) max_val = out[i];
}
float sum = 0.0f;
for (int i = 0; i < ML_OUTPUT_SIZE; i++) {
    out[i] = expf(out[i] - max_val);
    sum += out[i];
}
for (int i = 0; i < ML_OUTPUT_SIZE; i++) out[i] /= sum;
```

### Argmax: Picking the Winner

After softmax, argmax simply finds the index with the highest probability. That index is the predicted class:

```c
int best = 0;
for (int i = 1; i < ML_OUTPUT_SIZE; i++) {
    if (out[i] > out[best]) best = i;
}
```

If `out[0]` is largest, the prediction is `ML_CLASS_COMFORTABLE` (class ID 0). The confidence reported over BLE is `out[best] * 100`, clamped to a `uint8_t`.

**Key takeaway:** Softmax maps any vector of real numbers to a probability distribution; argmax picks the most likely class; and the winning probability is the confidence score.

---

## 3. Neural Networks from First Principles

### A Single Neuron

The basic unit of a neural network is a **neuron** (also called a unit or perceptron). It takes several inputs, multiplies each by a **weight**, sums the results, adds a **bias**, and passes the total through an **activation function**:

```
y = activation(w1*x1 + w2*x2 + ... + wn*xn + b)
```

Or in compact matrix notation:

```
y = activation(W · x + b)
```

Think of weights as the neuron's opinion about how important each input is. A large positive weight for temperature means "high temperature strongly activates this neuron." A large negative weight means "high temperature suppresses this neuron." The bias shifts the activation threshold — it lets the neuron fire even when all inputs are zero, or require a higher combined signal before it fires.

This is not that different from a hardware comparator with a programmable threshold — except the parameters W and b are learned from data, not hand-configured.

### Why Stack Multiple Neurons in Layers?

A single neuron can only learn a linear boundary — a straight line dividing two classes in input space. Stack multiple neurons in a layer and each learns a different linear feature. Stack multiple layers and each layer combines the previous layer's features into more abstract ones.

Think of it like building a multi-stage signal chain. The first layer in our model (16 neurons) looks directly at the three normalized sensor values and detects 16 different "micro-patterns" — things like "high temperature AND low humidity" or "pressure dropping." The second layer (8 neurons) combines those micro-patterns into 8 broader features. The output layer (5 neurons) combines those into class scores.

Each additional layer doesn't just add more neurons — it adds another level of abstraction. The representation that reaches the output layer is far richer than what any single layer could compute.

### ReLU: The Activation Function for Hidden Layers

Without activation functions, stacking linear layers would collapse into a single equivalent linear transformation — no matter how many layers you add, the whole network behaves like one layer. Activation functions break linearity.

**ReLU** (Rectified Linear Unit) is the standard choice for hidden layers today:

```
f(x) = max(0, x)
```

That is all it does. Negative inputs are clamped to zero; positive inputs pass through unchanged. In C:

```c
static void relu(float *x, int n)
{
    for (int i = 0; i < n; i++) {
        if (x[i] < 0.0f) x[i] = 0.0f;
    }
}
```

#### Why ReLU and not one of the alternatives?

This is the most-asked "why," so here is the full menu and why each was passed over for *this* network's hidden layers:

| Activation | Formula | Why not here |
|---|---|---|
| **Sigmoid** | `1/(1+e⁻ˣ)` | Squashes everything into (0, 1), so gradients shrink toward zero for large \|x\| → **vanishing gradients**, slow training. Also needs an `expf` per neuron — expensive if you ever ran it on-device. |
| **Tanh** | `(eˣ−e⁻ˣ)/(eˣ+e⁻ˣ)` | Zero-centred (better than sigmoid) but still saturates at ±1, so the same vanishing-gradient problem. Two `expf` per neuron. |
| **ReLU** ✅ | `max(0, x)` | No saturation for positive inputs → gradient is exactly 1 there, so the learning signal flows freely back to the first layer. One comparison per element — the cheapest non-linearity that exists. |
| **Leaky ReLU / ELU / GELU** | small slope or curve for x<0 | Solve the "dying ReLU" problem (below) but add parameters/compute. On a 3-input toy network with cleanly separated classes, they buy no measurable accuracy — added complexity for nothing. |

The deeper reason ReLU matters at all: without **any** non-linearity, stacking `Dense` layers collapses to a single linear map (`W₂(W₁x) = (W₂W₁)x`), so a 10-layer network would be no more expressive than one layer. ReLU's "kink" at 0 is the cheapest possible way to break that linearity and let the network bend decision boundaries.

**The one risk — "dying ReLU":** because the gradient is exactly 0 for negative inputs, a neuron that gets pushed firmly negative can stop learning permanently (it always outputs 0, so no gradient ever flows to fix it). This is why Leaky ReLU exists. It did **not** bite this project because the inputs are normalised to [0, 1], the network is shallow, and Adam keeps the weights well-scaled — so no neuron ever got stranded. On a deeper or more sensitive network, Leaky ReLU would be the first thing to try if accuracy stalled.

**Bottom line:** ReLU is the default *because* it is the simplest thing that trains well, and nothing about this problem justified a more expensive activation. "Use the cheapest tool that works" is the recurring TinyML theme.

### Our Architecture: 3 → 16 → 8 → 5

The architecture is defined in `train_classifier.py`:

```python
model = tf.keras.Sequential([
    tf.keras.layers.Input(shape=(3,)),
    tf.keras.layers.Dense(16, activation='relu'),
    tf.keras.layers.Dense(8,  activation='relu'),
    tf.keras.layers.Dense(5,  activation='softmax'),
])
```

Translated to a data flow diagram:

```
[temp, hum, press]     <- 3 normalized inputs
       |
  Dense(16, ReLU)      <- first hidden layer
       |
  Dense(8, ReLU)       <- second hidden layer
       |
  Dense(5, softmax)    <- output layer, one score per class
       |
[comfortable, warm, cold, humid, danger]
```

### Why This Many Layers and Neurons? (and why not more or fewer)

The code comment in `train_classifier.py` is blunt about it — *"Layer sizes (16, 8) chosen to balance accuracy vs. flash footprint."* Here is the reasoning behind every number, because picking architecture sizes is exactly the kind of decision a learner needs to be able to justify.

**Why the input layer is 3.** Not a choice — it equals the feature count: temperature, humidity, pressure. The input "layer" is just the data.

**Why the output layer is 5.** One neuron per *trained* class. This is forced by the problem (5 labels) and by softmax (one logit per class). The 6th class, `anomaly`, is **not** a neuron — it's derived from low confidence (§7). Adding a 6th output neuron would require labelled "anomaly" training examples, which by definition don't exist for "none of the above."

**Why two hidden layers, not one or three?**
- **One hidden layer** *can* approximate any function (the universal approximation theorem), but it often needs to be very wide to carve out several non-convex class regions. Two narrow layers express the same boundaries with far fewer weights, because the second layer composes the first layer's features instead of re-deriving them.
- **Three+ hidden layers** add depth this problem doesn't need. With only 3 inputs and 5 well-separated classes, a third layer adds parameters (more flash, more overfitting risk) for no accuracy gain. Depth pays off when features are hierarchical (pixels → edges → shapes); three scalar sensor readings have no such hierarchy.
- **Two** is the sweet spot: enough composition to handle the joint temp+humidity boundaries (§6), little enough to stay tiny and train in seconds.

**Why 16 then 8 (a shrinking funnel)?**
- **The funnel shape (16 → 8 → 5)** is a deliberate pattern: the first layer expands the 3 raw inputs into 16 "micro-patterns," then each subsequent layer *compresses* toward the 5-way decision. Expanding first gives the network room to detect many simple feature combinations; narrowing afterward forces it to keep only the patterns that actually discriminate classes. This is a gentle information bottleneck — it discourages memorising noise.
- **Why not 32 → 16 (bigger)?** It trains to the same ~99% accuracy here, but doubles the weight count (more flash) and raises overfitting risk on a dataset this small (~1,900 samples). On a microcontroller, bytes you don't need are bytes you don't spend.
- **Why not 8 → 4 (smaller)?** Tested informally, it still clears the 85% target, but margins on the hardest boundary (comfortable↔humid) get thinner and the model is more sensitive to the random seed. 16/8 leaves comfortable headroom (98.83% vs an 85% bar) for almost no cost.
- **Powers of two (16, 8)** are convention, not magic — they pack neatly and make the parameter arithmetic easy to follow. 14 and 7 would work just as well.

**How you'd actually choose these in practice.** There is no formula. The honest method is a small sweep: train a handful of candidates (`8/4`, `16/8`, `32/16`), compare *validation* accuracy and parameter count, and pick the smallest one that comfortably clears the target. The values here are the result of exactly that kind of "smallest model that clears 85% with margin" reasoning — embedded ML optimises for *fit-on-the-chip* first, accuracy-above-threshold second.

### Counting Parameters

Every connection between layers is a weight. Every neuron has one bias. Let us count them:

| Layer | Weights | Biases | Subtotal |
|-------|---------|--------|----------|
| Input (3) → Hidden 1 (16) | 3 × 16 = 48 | 16 | 64 |
| Hidden 1 (16) → Hidden 2 (8) | 16 × 8 = 128 | 8 | 136 |
| Hidden 2 (8) → Output (5) | 8 × 5 = 40 | 5 | 45 |
| **Total** | **216** | **29** | **245** |

245 parameters. Every single one of those numbers is stored in `ml_weights.h` as a `float` literal. You can count the values in `ML_W1[48]`, `ML_b1[16]`, `ML_W2[128]`, `ML_b2[8]`, `ML_W3[40]`, `ML_b3[5]` and verify this.

**Key takeaway:** A neural network is a cascade of matrix multiplications and element-wise nonlinearities. The parameters (weights and biases) are the numbers learned from data. Our 3-16-8-5 network has exactly 245 such parameters.

---

## 4. Training: Making the Network Learn

### The Loss Function

Training requires a way to measure how wrong the model is on a given example. This is the **loss function**. For multi-class classification we use **categorical cross-entropy**:

```
Loss = -log( predicted_probability_for_correct_class )
```

If the model predicts 0.9 probability for the correct class, loss = -log(0.9) ≈ 0.105 (small — good). If it predicts 0.1, loss = -log(0.1) ≈ 2.3 (large — bad). The log function makes the penalty grow sharply as confidence in the wrong answer increases.

The `compile` call in `train_classifier.py` specifies this:

```python
model.compile(optimizer='adam',
              loss='sparse_categorical_crossentropy',
              metrics=['accuracy'])
```

"Sparse" here just means labels are integers (0, 1, 2, 3, 4) rather than one-hot vectors — a memory optimization. The math is identical.

**Why cross-entropy and not mean squared error (MSE)?** MSE is the natural loss for *regression* (predicting a number), and you *could* bolt it onto a classifier — but it trains badly here for two reasons. First, paired with softmax, MSE produces a **non-convex, flat-gradient** loss surface: when the model is confidently wrong, the gradient is nearly zero, so it learns slowly exactly when it most needs to correct. Cross-entropy paired with softmax has the opposite, ideal property — the gradient is simply `(predicted − actual)`, large precisely when the prediction is far off. Second, cross-entropy is the **maximum-likelihood** loss for a categorical distribution: minimising it is mathematically equivalent to maximising the probability the model assigns to the correct labels, which is exactly what we want a classifier to do. MSE has no such interpretation for class probabilities.

### Gradient Descent

After computing the loss, the goal is to adjust the 245 weights to make it smaller. The tool for this is **gradient descent**.

Think of the loss as a high-dimensional landscape. Each weight is one dimension. The gradient of the loss with respect to each weight tells you the slope of that landscape at the current position — which direction is uphill. To minimize loss, step in the opposite direction (downhill):

```
weight_new = weight_old - learning_rate * gradient
```

The **learning rate** controls step size. Too large and the optimizer overshoots the minimum and diverges; too small and training takes forever.

### Backpropagation

The gradient needs to be computed for all 245 weights simultaneously. **Backpropagation** is the algorithm for doing this efficiently using the chain rule of calculus.

The chain rule says: if `loss` depends on `output` which depends on `hidden`, then `d(loss)/d(hidden) = d(loss)/d(output) * d(output)/d(hidden)`. Backpropagation applies this rule layer by layer, starting from the output layer and working backward through the network — hence "back" propagation.

You do not need to implement backprop. TensorFlow/Keras computes it automatically using automatic differentiation. But understanding the concept explains why you always need a differentiable architecture: ReLU is piecewise differentiable, softmax is differentiable, dense layers are differentiable. The gradient can flow all the way back to the first layer.

### Adam Optimizer

Vanilla gradient descent uses a fixed learning rate. **Adam** (Adaptive Moment Estimation) adapts the learning rate independently for each weight based on its history of gradients. Weights whose gradients are consistently large get a smaller effective learning rate (they are already making big steps). Weights with small or noisy gradients get a larger rate.

This has two practical effects:

1. Training converges faster — typically 3-5× fewer epochs than SGD with a fixed rate.
2. It is less sensitive to the initial learning rate choice — a major quality-of-life improvement.

Adam is the default optimizer for most classification tasks and is what `train_classifier.py` uses.

**Why Adam and not plain SGD, SGD+momentum, or RMSprop?**
- **Plain SGD** (fixed learning rate) works but is fussy — pick the rate too high and it diverges, too low and it crawls. You'd spend time hand-tuning it.
- **SGD + momentum** accelerates by accumulating a velocity across steps, but still uses one global learning rate for every weight.
- **RMSprop** adapts the rate *per weight* using a running average of squared gradients — half of what Adam does.
- **Adam** = RMSprop's per-weight scaling **plus** momentum, with bias correction for the first few steps. It is the most forgiving choice: it converges fast and is robust to the initial learning rate, so on a small project you train once and move on instead of babysitting hyperparameters. For a 245-weight model that trains in seconds, there is zero reason to trade that convenience away for a hand-tuned SGD.

### Epochs and Batch Size

The training call in `train_classifier.py`:

```python
model.fit(X_train, y_train, epochs=50, batch_size=32, validation_split=0.1, verbose=1)
```

- **Epoch:** One complete pass through the entire training dataset. After 50 epochs, the model has seen every training sample 50 times. The optimizer has had 50 opportunities to improve the weights based on every example.

- **Batch size:** Rather than computing the gradient on the entire dataset at once (expensive and memory-intensive) or on one sample at a time (noisy), you process 32 samples at a time. The gradient estimated from 32 samples is a good approximation of the true gradient and fits in CPU cache. One epoch of 1500 samples with batch_size=32 means about 47 gradient update steps.

- **validation_split=0.1:** 10% of training data is held out during training and used to measure performance on unseen data after each epoch. This lets you spot overfitting — when training accuracy keeps rising but validation accuracy stops improving.

### Why These Specific Values?

None of these numbers are sacred — they're the standard "small, well-behaved dataset" defaults, and the validation curve confirmed they were enough:

- **`epochs=50`** — enough passes for a 245-weight model on ~1,900 clean samples to plateau. You can tell it converged because validation accuracy flattens well before epoch 50; pushing to 200 wouldn't help and would risk overfitting. (A production pipeline would add *early stopping* to halt automatically when validation loss stops improving — overkill for a model that trains in seconds.)
- **`batch_size=32`** — the classic default. Big enough that each gradient estimate is stable (averaging 32 samples cancels noise), small enough that the optimiser takes ~47 update steps per epoch instead of one giant step. It also fits trivially in cache. 16 or 64 would train equally well; 1 (pure stochastic) would be noisy, the full 1,900 (batch gradient descent) would take fewer, coarser steps.
- **Learning rate** — not set explicitly, so Adam uses its default `0.001`. Because Adam adapts per-weight, this default "just works" across a huge range of problems; tuning it would be premature optimisation here.
- **`random_state=42` / fixed RNG seed** — makes the split and the synthetic data **reproducible**. Re-running the pipeline gives the identical dataset and split, so the committed `ml_weights.h` is regenerable and the 98.83% number is verifiable, not a lucky run. (42 is just the customary placeholder seed.)

The meta-point: these are *defaults chosen because the problem is easy*, and the validation accuracy is the evidence that they were sufficient. The discipline isn't picking magic numbers — it's watching the validation curve and only reaching for fancier machinery (schedules, early stopping, regularisation) when the curve tells you the defaults fell short.

### Train/Test Split and Overfitting

```python
X_train, X_test, y_train, y_test = train_test_split(X, y, test_size=0.2, stratify=y, random_state=42)
```

The 80/20 split reserves 20% of samples as a final holdout test set, never seen during training. **Overfitting** is when a model memorizes the training data instead of learning the underlying pattern — it performs perfectly on training data but poorly on new data. By evaluating on the held-out test set, you get an honest estimate of generalization performance.

`stratify=y` ensures each class is proportionally represented in both sets — important because without it, random splitting might put all "danger" samples in training by accident.

The trained model in `ml_weights.h` achieves 98.83% accuracy on the test set (recorded in the weights file header comment). Our target was 85%. The result is well above threshold because the class boundaries in the synthetic data are clearly separated with only moderate overlap at the edges.

**Key takeaway:** Training minimizes a loss function via gradient descent, backpropagation computes how each weight contributed to the error, and Adam makes convergence fast. The 80/20 train/test split validates that the model generalizes beyond its training data.

---

## 5. TinyML: ML on Microcontrollers

### The Constraints

Running ML inference on a microcontroller is not like running it on a server. The ESP32-C3 has:

- **400 KB SRAM** — your entire stack, heap, FreeRTOS, NimBLE host stack, sensor buffers, and display framebuffer compete for this
- **No operating system** in the Unix sense — FreeRTOS is an RTOS with basic task scheduling, not a full OS
- **No floating-point hardware accelerator** (the ESP32-C3 has a hardware FPU, but no SIMD or ML accelerator)
- **4 MB flash** — firmware, NVS, and model weights must all fit here

### Two Approaches to On-Device Inference

**Approach A: TFLite Micro runtime + quantized model**

TensorFlow Lite for Microcontrollers is Google's framework for running inference on constrained devices. You export a TFLite model, quantize it, and embed the `.tflite` flatbuffer. The TFLite Micro runtime interprets the flatbuffer at runtime and dispatches each operation.

The advantages are broad operator support, quantization infrastructure, and official hardware-specific optimizations. The disadvantage is runtime overhead: the TFLite Micro library itself is 20–60 KB depending on which operators you need, and it requires the model to be embedded as a byte array and interpreted at runtime.

**Approach B: Pure-C forward pass with embedded weights**

For a small, fixed architecture, you can skip the runtime entirely and write the forward pass as direct C code. The weights become compile-time constants in a header file. There is no interpreter, no flatbuffer parsing, no operator dispatch table — just arithmetic.

### Quantization (Why Approach A Cares)

Training uses 32-bit floats. Each weight is 4 bytes. **Quantization** maps float32 weights to int8 using a linear scale factor: `quantized = round(float / scale)`. This reduces model size by 4× (from 4 bytes to 1 byte per weight) at a small accuracy cost — typically less than 1%.

For our 245-weight model, the memory difference is:
- float32: 245 × 4 = 980 bytes
- int8: 245 × 1 = 245 bytes

The saving is ~735 bytes. On a 400 KB device, this is negligible. Quantization would matter if the model were thousands of times larger.

### Why This Project Uses Approach B

Three reasons:

1. **TFLite Micro is not in the ESP-IDF v5.2.3 component registry.** Integrating it requires manually adding a CMake component with a large external source tree. For a model this small, that complexity buys nothing.

2. **The model is tiny.** 245 parameters at float32 = 980 bytes. The entire `tinyml_inference.c` + `ml_weights.h` adds approximately 5 KB to the firmware binary. The total firmware image is `0x95d60` bytes (around 614 KB). ML is less than 1% of the binary footprint.

3. **Zero runtime overhead.** The forward pass is three matrix multiplications, two ReLU passes, and one softmax. On the ESP32-C3 at 160 MHz, this completes in microseconds. No interpreter, no memory allocation, no context switching.

`train_classifier.py` does still export a TFLite model (`models/model.tflite`) as an artifact, but the firmware never uses it. A separate `ml/extract_weights.py` script reads the TensorFlow SavedModel, extracts the numpy arrays, and writes them as C float literals into `ml_weights.h`.

**Key takeaway:** TinyML on microcontrollers means choosing between a portable runtime (TFLite Micro) and hand-written inference code. For models under ~1000 parameters on a platform without a component registry entry, pure-C forward pass is simpler, smaller, and faster.

---

## 6. This Project's Model in Detail

### Input Normalization

Neural networks train best when all inputs are in a similar range — ideally [0, 1] or [-1, 1]. Without normalization, a feature measured in thousands (pressure in Pa) would dominate gradient updates over a feature measured in tens (temperature in °C), causing slow or unstable training.

The normalization formula is min-max scaling:

```
normalized = (value - min) / (max - min)
```

The NORM dictionary in `train_classifier.py` defines the ranges:

```python
NORM = {'temp_c': (-10, 60), 'humidity_pct': (0, 100), 'pressure_hpa': (900, 1100)}
```

These ranges cover the full physical sensor slider range per the GATT profile (b7e00006: temp −10–60°C, humidity 0–100%, pressure 900–1100 hPa). Any value inside these ranges normalizes to [0, 1]. Values outside the range would normalize outside [0, 1] — the model would be extrapolating, and confidence typically drops, which is one mechanism that triggers anomaly detection.

**Why min-max scaling and not z-score standardization?** The other common technique, *standardization*, subtracts the mean and divides by the standard deviation (`(x − μ) / σ`), giving each feature mean 0 and variance 1. Both work for training, but min-max wins **for this embedded deployment** for two concrete reasons:
1. **The valid range is physically fixed and known.** A pressure sensor's output is bounded by physics (≈900–1100 hPa), not by whatever happened to be in the training set. Min-max encodes that real-world domain knowledge directly; z-score would tie the scaling to the training data's particular μ and σ, which shift every time you collect new data.
2. **Out-of-range inputs become a free anomaly signal.** With min-max, a reading outside the physical range lands outside [0, 1], pushing the model into extrapolation territory where confidence drops — feeding the §7 anomaly check naturally. With z-score, an extreme value just becomes a large positive/negative number with no clean "out of bounds" meaning.

**Why are the ranges hard-coded constants instead of computed from the data?** Because the firmware must apply the *exact same* transform with no access to the training set. The denominators `70.0 / 100.0 / 200.0` are compiled into `tinyml_inference.c`; if training used data-derived statistics, every retrain would silently change them and you'd have to keep firmware and Python in lockstep by hand. Fixed physical ranges make the contract explicit and auditable — which is why `train_classifier.py` flags the `NORM` dict with *"MUST match … tinyml_inference.c."*

The firmware applies the exact same normalization before inference:

```c
float input[ML_INPUT_SIZE] = {
    (temp_c       - (-10.0f)) / 70.0f,
    (humidity_pct -   0.0f)  / 100.0f,
    (pressure_hpa - 900.0f)  / 200.0f,
};
```

The denominators (70.0, 100.0, 200.0) are the range widths: 60−(−10) = 70, 100−0 = 100, 1100−900 = 200. This must match the training NORM exactly — even a small discrepancy would shift all inputs into a different region of the model's learned space and produce wrong predictions.

### The Synthetic Training Data

`collect_synthetic.py` generates 300 samples per class with uniform random sampling:

```python
samples(300, (18, 25), (30, 60), (1000, 1025), 'comfortable'),
samples(300, (28, 45), (20, 50), (990, 1020),  'warm'),
samples(300, (-5, 15), (20, 50), (985, 1015),  'cold'),
samples(300, (18, 28), (75, 99), (995, 1020),  'humid'),
samples(300, (46, 60), (0,  20), (970, 995),   'danger'),
```

Notice that boundaries overlap deliberately. Temperature 18–25°C for `comfortable` and 18–28°C for `humid` share the range 18–25°C. The distinguishing feature is humidity (30–60% for comfortable vs 75–99% for humid). The model must learn that classification depends on the joint combination, not any single feature.

The `danger` class uses temperatures 46–60°C with very low humidity (0–20%) and lower pressure (970–995 hPa) — the combination you would see in an overheating enclosure in low-altitude conditions. No single threshold can separate this from `warm` on temperature alone.

Total training data: 1500 synthetic + real sensor CSV files (`ble_env_*.csv`, 379 samples with ±2°C/±2%/±2 hPa drift applied at collection time) = 1879 samples total. The drift makes training data realistic by mimicking the sensor noise the firmware applies in simulation mode.

### Walking Through `tinyml_infer()` Line by Line

```c
ml_result_t tinyml_infer(float temp_c, float humidity_pct, float pressure_hpa)
{
```
Inputs come from `telemetry_task` in `app_main.c`, already converted from fixed-point to float.

```c
    float input[ML_INPUT_SIZE] = {
        (temp_c       - (-10.0f)) / 70.0f,
        (humidity_pct -   0.0f)  / 100.0f,
        (pressure_hpa - 900.0f)  / 200.0f,
    };
```
Normalize to [0, 1] matching the training NORM. After this, a reading of 22°C, 45%, 1013 hPa becomes approximately [0.457, 0.450, 0.565].

```c
    float h1[ML_LAYER1_SIZE];
    float h2[ML_LAYER2_SIZE];
    float out[ML_OUTPUT_SIZE];

    dense(input, ML_INPUT_SIZE,  ML_W1, ML_b1, h1, ML_LAYER1_SIZE);
    relu(h1, ML_LAYER1_SIZE);
```
The first dense layer: multiply the 3-element input by the 16×3 weight matrix ML_W1, add bias ML_b1. Each of the 16 output values is `dot(row_i_of_W1, input) + b1[i]`. Then ReLU zeroes out any negative values.

```c
    dense(h1, ML_LAYER1_SIZE, ML_W2, ML_b2, h2, ML_LAYER2_SIZE);
    relu(h2, ML_LAYER2_SIZE);
```
Second dense layer: 8×16 matrix multiply + bias + ReLU. The 16 features from h1 are compressed to 8 higher-level features.

```c
    dense(h2, ML_LAYER2_SIZE, ML_W3, ML_b3, out, ML_OUTPUT_SIZE);
```
Output layer: 5×8 matrix multiply + bias. No activation yet — these are raw logits.

```c
    /* Softmax — numerically stable */
    float max_val = out[0];
    for (int i = 1; i < ML_OUTPUT_SIZE; i++) {
        if (out[i] > max_val) max_val = out[i];
    }
    float sum = 0.0f;
    for (int i = 0; i < ML_OUTPUT_SIZE; i++) {
        out[i] = expf(out[i] - max_val);
        sum += out[i];
    }
    for (int i = 0; i < ML_OUTPUT_SIZE; i++) out[i] /= sum;
```
Softmax converts logits to probabilities. Subtracting `max_val` before `expf` prevents overflow (if raw logits are large, `expf` can return infinity on a 32-bit float).

```c
    /* Argmax */
    int best = 0;
    for (int i = 1; i < ML_OUTPUT_SIZE; i++) {
        if (out[i] > out[best]) best = i;
    }

    /* Anomaly check */
    if (out[best] < 0.50f) {
        uint8_t conf = (uint8_t)((1.0f - out[best]) * 100.0f);
        return (ml_result_t){ .class_id = ML_CLASS_ANOMALY, .confidence = conf };
    }

    return (ml_result_t){
        .class_id   = (ml_class_t)best,
        .confidence = (uint8_t)(out[best] * 100.0f),
    };
}
```
If the highest-confidence class is below 50%, the model is uncertain — return `ML_CLASS_ANOMALY`. Otherwise, return the predicted class with confidence as a 0–100 integer.

The `dense()` helper:

```c
static void dense(const float *in, int in_size,
                  const float *W, const float *b,
                  float *out, int out_size)
{
    for (int i = 0; i < out_size; i++) {
        float acc = b[i];
        for (int j = 0; j < in_size; j++) {
            acc += W[i * in_size + j] * in[j];
        }
        out[i] = acc;
    }
}
```

This is a row-major matrix-vector multiply. `W[i * in_size + j]` is element (i, j) of the weight matrix — the connection from input neuron j to output neuron i. The weights are stored in row-major order, exactly as numpy exports them, which is why `extract_weights.py` can write them as a flat C array.

**Key takeaway:** The firmware's forward pass is three dense() calls, two relu() calls, and one softmax — about 50 lines of C implementing the entire inference pipeline. Every constant in ml_weights.h was produced by `train_classifier.py` and `extract_weights.py`.

---

## 7. Anomaly Detection

### What "Anomaly" Means Here

Class 5, `ML_CLASS_ANOMALY`, is not a class the model was trained to recognize. It is a derived output — a signal that the classifier is uncertain.

The logic is: if no class achieves more than 50% softmax probability, the input does not clearly belong to any known class.

```c
if (out[best] < 0.50f) {
    return (ml_result_t){ .class_id = ML_CLASS_ANOMALY, ... };
}
```

This works because softmax probabilities sum to 1. If the five class probabilities are all near 0.2 — near-uniform distribution — that means the input is equidistant from all class centroids in the model's learned feature space. No single class wins confidently. This pattern emerges for inputs near class boundaries or for inputs outside the training distribution entirely.

### Why the Confidence Threshold Works

Consider the class boundary between `comfortable` and `humid`. `Comfortable` occupies temperature 18–25°C with humidity 30–60%. `Humid` occupies temperature 18–28°C with humidity 75–99%. An input of 25°C, 67% humidity sits exactly between them.

When you run inference on 25°C, 67% humidity, the model might produce softmax output like [0.35, 0.05, 0.02, 0.40, 0.00] — comfortable and humid split the vote. The best class is `humid` at 40%, but 40% < 50%, so the firmware returns `ML_CLASS_ANOMALY`.

This is semantically correct: a reading of 25°C, 67% humidity is a genuinely ambiguous condition that does not cleanly match any of the five labeled classes. Flagging it as an anomaly alerts the application rather than silently picking the wrong class.

The confidence value reported for an anomaly is `(1.0 - out[best]) * 100`. This is the "degree of uncertainty" — how far the best class was from being confident. A best class probability of 0.49 gives an anomaly confidence of 51% (almost triggered normal classification). A best class probability of 0.20 gives 80% (strongly anomalous).

### Why 0.50 and Not Some Other Threshold?

0.50 is a principled floor, not an arbitrary tuning knob, and the reason is baked into how softmax behaves with 5 classes:

- **It's the "is one class winning the majority?" line.** Five probabilities sum to 1. For any class to exceed 0.50, it must hold more probability mass than *all four others combined* — an unambiguous winner. Below 0.50, by definition at least two classes are sharing the vote, which is precisely the "I'm not sure" state we want to flag.
- **Why not lower (e.g. 0.35)?** A well-trained, confident in-distribution prediction here sits at 0.95–0.99 (see the 98.83% accuracy). The gap between "confident" (~0.99) and "genuinely split" (~0.40) is wide, so the exact cut can sit anywhere in that valley. Drop it to 0.35 and you'd let through readings where the top class barely edges out a rival — more false negatives (missed anomalies).
- **Why not higher (e.g. 0.70)?** You'd start flagging perfectly good predictions that happen to sit near a soft boundary at, say, 0.65 — more false positives (nuisance alerts). 0.50 is the natural midpoint that keeps both error types low *given how separated these classes are*. If the classes overlapped more, you'd lower it; if they were razor-sharp, you could raise it.

The takeaway is that the threshold is tied to the **class separation in the data**, not chosen blindly — and for cleanly separated classes, the majority line (0.50) is the obvious, defensible choice.

### Why Derive Anomaly From Confidence Instead of Training a 6th "None" Class?

A tempting alternative: add a sixth output neuron for "anomaly" and train it. This **cannot work**, and the reason is a genuinely important ML concept called **open-set recognition**:

- To train a class, you need labelled examples of it. But "anomaly" means *"none of the known classes"* — an open-ended, infinite set you cannot enumerate or sample. There is no finite training set for "everything that isn't one of these five."
- Even if you collected some odd readings and labelled them "anomaly," the model would only learn to recognise *those particular* oddities, not the unbounded space of future ones. The first genuinely novel input would be misclassified.
- The confidence-threshold approach sidesteps this entirely: it doesn't try to *learn* what anomalies look like — it infers anomaly from the absence of a confident known answer. The model only ever learns the five things it *can* be shown, and "unknown" falls out of the math for free. (This is also exactly why the earlier autoencoder failed, below — it tried to *model* normality and ended up rejecting valid non-comfortable classes.)

### Why the Autoencoder Approach Was Abandoned

An autoencoder is a different kind of model: it learns to compress and reconstruct its input. Trained only on `comfortable` examples, it learns what comfortable sensor readings look like. At inference time, you run the input through the encoder and decoder, compute the reconstruction error, and flag anomalies when the error exceeds a threshold.

The original autoencoder arrays (`ML_AE_We`, `ML_AE_Wd`, etc.) and the `ML_ANOMALY_THRESHOLD = 0.00474350f` macro were carried in `ml_weights.h` for a while as dead code (the linker stripped them via `--gc-sections`, so they had zero binary cost). They were deleted in commit `85571ca` (REVIEW_FINDINGS B5).

The problem: the autoencoder was trained only on comfortable-temperature and comfortable-humidity ranges. Any non-comfortable reading — warm, cold, humid, danger — produces high reconstruction error. The model was flagging every non-comfortable reading as an anomaly, making it useless for distinguishing "sensor reading outside known training distribution" from "sensor reading in a known-but-non-comfortable class."

The classifier-confidence approach is strictly better for this use case because the classifier knows about all five classes. It only flags anomaly when the input does not fit any of them — not just when it does not fit one.

### How to Trigger an Anomaly in Testing

Send a sensor override via b7e00006 with values that sit between class boundaries:

- Temperature: 25–27°C (comfortable/warm boundary)
- Humidity: 60–75% (comfortable/humid boundary)
- Pressure: 1000–1010 hPa

At these values, the model's softmax output tends to split between `comfortable`, `warm`, and `humid`, with no class exceeding 50%. The firmware logs `ML class changed → 5 (conf XX%)` and sends a BLE notification on b7e00007 with class byte = 0x05.

**Key takeaway:** Anomaly detection here is classifier-confidence thresholding — a simple and effective technique when you have a well-trained multi-class model. The threshold of 50% was chosen because softmax probabilities sum to 1; a confident model always puts well over 50% on one class for in-distribution inputs.

---

## 8. End-to-End Flow in This Project

This section traces a single inference event from the Android UI through the firmware and back to the app.

### Step 1: Android Slider → BLE Write

The Android companion app's `SensorScreen.kt` presents sliders for temperature, humidity, and pressure. When the user moves a slider, the app encodes the values into a 6-byte little-endian payload matching the b7e00006 Sensor Override Characteristic:

| Bytes | Field | Encoding |
|-------|-------|----------|
| 0–1 | temperature | int16, °C × 100 |
| 2–3 | humidity | uint16, % × 100 |
| 4–5 | pressure | uint16, hPa × 10 |

The app writes this payload to UUID `b7e00006-4f4a-4c2a-8b7d-2f6a6c000000` over an encrypted BLE connection (NimBLE enforces ATT error 0x05 if the link is unencrypted).

### Step 2: `sensor_provider_set_override()` → Drift

NimBLE calls `gatt_access_cb` in `ble_env_service.c` when the ATT write arrives. The callback decodes the 6-byte payload and calls `sensor_provider_set_override()` in `sensor_provider.c`.

The sensor provider stores these values as the "override" reading. On each read, it adds time-based drift of ±2°C, ±2%, and ±2 hPa. This simulates realistic sensor noise and ensures the inference engine is tested against the same variation the training data was designed for.

### Step 3: `sensor_provider_read()` in `telemetry_task`

In `app_main.c`, `telemetry_task` runs continuously:

```c
static void telemetry_task(void *arg)
{
    while (true) {
        app_state_t state = app_state_get_snapshot();
        sensor_sample_t sample = sensor_provider_read();
        ...
        /* TinyML inference — notify b7e00007 only on class change */
        static ml_class_t s_last_class = ML_CLASS_COMFORTABLE;
        ml_result_t ml = tinyml_infer(
            sample.temperature_c_x100 / 100.0f,
            sample.humidity_pct_x100  / 100.0f,
            sample.pressure_pa        / 100.0f   /* Pa → hPa */
        );
```

The sensor returns fixed-point values (temperature as int16 in units of 0.01°C, pressure in Pa). These are converted to float before passing to `tinyml_infer()`. Pressure is divided by 100 to convert from Pa to hPa, matching the model's training units.

### Step 4: `tinyml_infer()` Runs the Forward Pass

As described in Section 6, three dense layers + softmax run in microseconds. The result is an `ml_result_t` with `class_id` and `confidence`.

### Step 5: On-Change BLE Notification to b7e00007

```c
        if (ml.class_id != s_last_class) {
            s_last_class = ml.class_id;
            ble_env_service_notify_ml_alert((uint8_t)ml.class_id, ml.confidence);
            ESP_LOGI(TAG, "ML class changed → %d (conf %u%%)", ml.class_id, ml.confidence);
        }
```

The static `s_last_class` variable means the BLE notification on `b7e00007` is sent **only when the class changes**. This is intentional: if the environment stays "comfortable" across 100 consecutive samples, there is no point sending 100 identical BLE notifications. The Android app is already displaying the last-known class.

`ble_env_service_notify_ml_alert()` in `ble_env_service.c` packs a 2-byte payload and calls `ble_gatts_notify()`:

| Byte | Field |
|------|-------|
| 0 | class_id (0–5) |
| 1 | confidence (0–100) |

This matches the b7e00007 ML Alert Characteristic payload defined in the GATT profile.

### Step 6: Android `DataAlertsScreen.kt` Receives the Notification

The Android app subscribes to b7e00007 notifications. When a notification arrives, `DataAlertsScreen.kt` reads byte 0 as the class ID and byte 1 as confidence, then displays a human-readable label and confidence percentage.

### Summary of the Full Data Path

```
Android slider (SensorScreen.kt)
    |-- BLE encrypted write --> b7e00006
    |
ESP32-C3 gatt_access_cb (ble_env_service.c)
    |-- sensor_provider_set_override()
    |
sensor_provider_read()  (with ±2° drift)
    |
telemetry_task (app_main.c, every report_interval_ms)
    |-- tinyml_infer(temp, hum, press)
    |   |-- normalize to [0,1]
    |   |-- dense(W1) + relu
    |   |-- dense(W2) + relu
    |   |-- dense(W3) + softmax
    |   `-- anomaly check (< 50% confidence)
    |
    if class changed:
        |-- ble_env_service_notify_ml_alert()
        |   `-- BLE notify --> b7e00007
        |
Android DataAlertsScreen.kt
    `-- display class label + confidence %
```

**Key takeaway:** The entire inference pipeline is synchronous and in-task. There are no background threads, no dynamic allocation, and no blocking calls. The forward pass completes before `vTaskDelay()` suspends the telemetry task, so latency is bounded by the report interval (default 2000 ms).

---

## Appendix: Quick Reference

### Architecture at a Glance

| Property | Value |
|----------|-------|
| Architecture | 3 → Dense(16, ReLU) → Dense(8, ReLU) → Dense(5, softmax) |
| Parameters | 245 total (216 weights + 29 biases) |
| Memory (float32) | ~980 bytes |
| Training samples | 1879 (1500 synthetic + 379 real) |
| Test accuracy | 98.83% |
| Anomaly threshold | max softmax probability < 0.50 |
| Inference latency | < 1 ms on ESP32-C3 at 160 MHz |
| Firmware footprint added | ~5 KB |

### Design Choices and Why the Alternatives Were Rejected

A one-glance map of every "why this and not that" in this guide. The point of learning a technology deeply is being able to defend each row.

| Decision | Chosen | Rejected alternatives | Core reason |
|---|---|---|---|
| Model type | Small MLP classifier | if-else rules; decision tree | Boundaries depend on the *joint* combination of noisy features (§1) |
| Problem framing | 5-class, single-label | regression; multi-label | A reading is exactly one category → softmax, not per-class sigmoids (§2) |
| Hidden activation | ReLU | sigmoid, tanh, Leaky/ELU/GELU | Cheapest non-linearity that trains without vanishing gradients (§3) |
| Depth | 2 hidden layers | 1 (needs to be wide); 3+ (no gain) | Two narrow layers compose features; no feature hierarchy to justify more (§3) |
| Width | 16 → 8 (funnel) | 32→16 (bigger); 8→4 (smaller) | Smallest size that clears 85% with margin; flash-first thinking (§3) |
| Output activation | Softmax | independent sigmoids | Classes are mutually exclusive and share a probability budget (§2) |
| Loss | Cross-entropy | MSE | Ideal gradient with softmax; it's the max-likelihood loss for categories (§4) |
| Optimizer | Adam | SGD, SGD+momentum, RMSprop | Per-weight adaptive rate + momentum → train once, no babysitting (§4) |
| Normalization | Min-max, fixed physical ranges | z-score; data-derived stats | Encodes known sensor bounds; firmware/Python stay in lockstep; free out-of-range signal (§6) |
| Inference runtime | Pure-C forward pass | TFLite Micro | 245 weights don't justify a ~100 KB interpreter; not in IDF registry (§5, DD-018) |
| Weight precision | float32 in a header | int8 quantization | ~735-byte saving is negligible on 400 KB SRAM; keeps full accuracy (§5) |
| Anomaly detection | Confidence threshold < 0.50 | dedicated 6th class; autoencoder | Open-set: "unknown" has no training set; autoencoder rejects valid classes (§7, DD-019) |
| Anomaly threshold | 0.50 | 0.35 (misses); 0.70 (false alarms) | The majority line, sized to this data's class separation (§7) |

> The design-decision records `../design_decisions.md` (DD-018 / DD-019) are the canonical source for the runtime and anomaly choices; this table is the learner-facing summary.

### Files You Need to Understand the Full System

| File | Role |
|------|------|
| `ml/collect_synthetic.py` | Generates training data with class ranges |
| `ml/train_classifier.py` | Trains the model and exports weights |
| `firmware/components/tinyml_inference/include/ml_weights.h` | Embedded float32 weights |
| `firmware/components/tinyml_inference/include/tinyml_inference.h` | Public API (`tinyml_infer`) |
| `firmware/components/tinyml_inference/tinyml_inference.c` | Forward pass implementation |
| `firmware/main/app_main.c` | Where inference is called in `telemetry_task` |
| `docs/gatt_profile.md` | b7e00007 ML Alert Characteristic payload format |

### The One Equation That Runs on the Device

At inference time, the firmware computes:

```
input_norm = (raw_input - feature_min) / feature_range
h1 = ReLU(W1 @ input_norm + b1)
h2 = ReLU(W2 @ h1 + b2)
logits = W3 @ h2 + b3
probs = softmax(logits)
class = argmax(probs)  if max(probs) >= 0.50 else ANOMALY
```

Everything else in this guide is context for why these six lines work.
