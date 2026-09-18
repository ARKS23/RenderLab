# Shader 编程思维速查

> 目标：从传统 CPU / C++ 的“控制流程思维”，逐渐切换到 GPU Shader 的“并行数据流 + 局部计算 + 数学函数 + Mask”思维。

---

# 1. Shader 编程和普通编程最大的区别

普通 CPU 程序更习惯：

```cpp
for (Object& Object : Objects)
{
    if (Object.IsEnemy())
    {
        if (Object.IsVisible())
        {
            DrawOutline(Object);
        }
    }
}
```

思维方式通常是：

```text
对象
 ↓
判断状态
 ↓
控制程序流程
 ↓
执行某个操作
```

Shader 更常见的写法是：

```hlsl
float EnemyMask   = ...;
float VisibleMask = ...;
float EdgeMask    = ...;

float OutlineMask =
    EnemyMask *
    VisibleMask *
    EdgeMask;
```

思维方式变成：

```text
输入数据
 ↓
提取特征
 ↓
构造 Mask
 ↓
组合 Mask
 ↓
生成输出
```

因此可以这样理解：

> CPU 编程更关注：“接下来执行什么？”

> Shader 编程更关注：“当前这个 Invocation 应该计算出什么值？”

---

# 2. 把 Shader 看成一个数学函数

Fragment Shader 可以粗略理解为：

\[
C_{\text{out}}
=
f(
UV,
P,
N,
Depth,
Material,
Light,
SceneData,
\ldots
)
\]

其中：

- \(UV\)：纹理坐标
- \(P\)：位置
- \(N\)：法线
- `Depth`：深度
- `Material`：材质参数
- `Light`：光源参数

屏幕上的每一个 Pixel 都会执行类似的 Shader：

```text
当前 Pixel 的输入
       ↓
执行 Shader
       ↓
得到当前 Pixel 的输出
```

例如：

```hlsl
float3 Color = BaseColor * Lighting;
return Color;
```

本质上就是：

\[
Color=f(BaseColor,Lighting)
\]

所以写 Shader 时优先思考：

> 我拥有哪些输入？

> 我要计算什么输出？

而不是：

> 程序下一步应该跳到哪里？

---

# 3. Invocation：Shader 最重要的心智模型

GPU 会并行执行大量 Shader Invocation。

例如 Fragment Shader：

```text
Pixel 0 → Shader Invocation
Pixel 1 → Shader Invocation
Pixel 2 → Shader Invocation
Pixel 3 → Shader Invocation
...
```

每个 Invocation 默认只负责自己的数据。

例如当前 Fragment 可能拥有：

```text
Position
Normal
UV
Depth
Material
Vertex Attributes
```

但它不会天然知道：

```text
左边 Pixel 是谁
右边 Pixel 是谁
场景里一共有多少 Actor
附近有没有敌人
其他 Mesh 在什么位置
```

如果需要额外信息，就必须显式读取：

```text
Texture
SceneTexture
Constant Buffer
Structured Buffer
Storage Buffer
Acceleration Structure
Shared Memory
```

因此更准确的理解是：

> Shader 不是“没有全局信息”，而是只能访问显式提供给 GPU 的数据。

---

# 4. 写 Shader 前先问四个问题

遇到一个 Shader 问题时，先不要急着写代码。

先回答下面四个问题：

## 4.1 当前 Invocation 是谁？

例如：

```text
Pixel
Vertex
Compute Thread
Particle
Object
Meshlet
Ray
```

## 4.2 当前已经拥有什么输入？

例如：

```text
UV
Normal
Position
Depth
Stencil
Thread ID
Material Data
```

## 4.3 我要输出什么？

例如：

```text
Color
Mask
Position
Visibility
Depth
Buffer Data
```

## 4.4 缺少的信息从哪里获得？

例如：

```text
Neighbor Pixel
SceneTexture
GBuffer
StructuredBuffer
Shadow Map
Ray Query
```

---

例如 UE 后处理描边：

```text
当前 Invocation
→ 当前 Pixel

已有信息
→ 当前 CustomStencil

缺少的信息
→ 周围 Pixel 的 CustomStencil

获取方式
→ SceneTextureFetch

目标
→ 判断当前 Pixel 是否是目标边缘

输出
→ 0 / 1 Outline Mask
```

---

# 5. Shader 最核心的思想之一：Mask

Shader 中大量逻辑最终都可以转换成一个 Mask：

\[
M\in[0,1]
\]

其中：

```text
0
→ 不满足条件

1
→ 满足条件
```

Mask 也可以是连续值：

\[
0 < M < 1
\]

这时通常表示：

```text
权重
透明度
混合比例
影响程度
```

例如：

```hlsl
float Mask =
    Condition ? 1.0f : 0.0f;
```

一旦把条件转换成 Mask，后面的很多逻辑就可以使用数学运算完成。

---

# 6. 把逻辑运算转换成 Mask 运算

假设：

\[
A,B\in\{0,1\}
\]

---

## 6.1 NOT

普通逻辑：

```cpp
!A
```

Mask 表达：

\[
1-A
\]

Shader：

```hlsl
float Result = 1.0f - A;
```

---

## 6.2 AND

普通逻辑：

```cpp
A && B
```

Mask 表达：

\[
A\cdot B
\]

Shader：

```hlsl
float Result = A * B;
```

只有：

```text
A = 1
B = 1
```

结果才是：

```text
1
```

---

## 6.3 OR

普通逻辑：

```cpp
A || B
```

Mask 表达：

\[
\max(A,B)
\]

Shader：

```hlsl
float Result = max(A, B);
```

只要其中一个为 `1`，结果就是 `1`。

---

## 6.4 多条件组合

普通 CPU 思维：

```cpp
if (!Center && Neighbor && Visible)
{
    Outline = 1;
}
```

Shader 可以写成：

```hlsl
float Outline =
    (1.0f - Center) *
    Neighbor *
    Visible;
```

数学形式：

\[
Outline=(1-C)NV
\]

---

# 7. Lerp：Shader 中非常重要的条件选择

CPU 代码：

```cpp
if (Mask)
{
    Color = Red;
}
else
{
    Color = Blue;
}
```

Shader 可以写成：

```hlsl
Color = lerp(Blue, Red, Mask);
```

`lerp` 的数学形式是：

\[
Result=(1-M)A+MB
\]

当：

\[
M=0
\]

有：

\[
Result=A
\]

当：

\[
M=1
\]

有：

\[
Result=B
\]

所以可以记成：

```text
If / Else
   ↓
Mask
   ↓
Lerp
```

---

# 8. Step：把连续数据转换成 0 / 1

普通逻辑：

```cpp
if (X >= 0.5f)
{
    Result = 1.0f;
}
else
{
    Result = 0.0f;
}
```

Shader 可以写：

```hlsl
float Result = step(0.5f, X);
```

其含义近似为：

\[
step(a,x)=
\begin{cases}
0,&x<a\\
1,&x\ge a
\end{cases}
\]

常见用途：

```text
Dissolve
Threshold Mask
高度判断
区域划分
二值化
```

---

# 9. SmoothStep：平滑阈值

`step` 会产生硬边：

```text
000000|111111
```

而：

```hlsl
float Mask =
    smoothstep(0.4f, 0.6f, X);
```

会得到平滑过渡：

```text
0 → 0.2 → 0.5 → 0.8 → 1
```

适合：

```text
柔和边缘
渐变 Mask
抗锯齿
程序化材质
软过渡
```

可以这样记：

```text
硬判断
→ step / If

平滑判断
→ smoothstep

连续混合
→ lerp
```

---

# 10. Saturate：把数据限制到 0～1

HLSL：

```hlsl
float X = saturate(Value);
```

等价于：

```hlsl
float X = clamp(Value, 0.0f, 1.0f);
```

数学形式：

\[
x=
\min
\left(
1,
\max(0,x)
\right)
\]

典型用途：

```text
Mask
Lighting
Dot Product
Blend Weight
Attenuation
```

例如 Lambert 光照：

```hlsl
float NdotL =
    saturate(dot(N, L));
```

---

# 11. Dot：建立方向关系

点积：

\[
\mathbf A\cdot\mathbf B
=
|\mathbf A|
|\mathbf B|
\cos\theta
\]

如果两个向量已经归一化：

\[
\mathbf A\cdot\mathbf B
=
\cos\theta
\]

因此：

```text
dot ≈ 1
→ 基本同方向

dot ≈ 0
→ 基本垂直

dot ≈ -1
→ 基本反方向
```

Shader 中大量效果依赖 `dot`：

```text
Lambert Lighting
Fresnel
Facing Ratio
Normal Mapping
Spot Light
Reflection
Rim Lighting
```

看到：

```hlsl
dot(A, B)
```

时可以先想：

> 这是在判断两个方向之间的关系。

---

# 12. Length / Distance：把空间关系转换成数值

例如：

```hlsl
float D =
    distance(WorldPos, LightPos);
```

数学上：

\[
d=
\left|
P-L
\right|
\]

然后可以构造一个简单的衰减 Mask：

```hlsl
float Attenuation =
    saturate(
        1.0f - D / Radius
    );
```

即：

\[
M=
saturate
\left(
1-\frac{d}{R}
\right)
\]

常见流程：

```text
空间位置
 ↓
距离
 ↓
归一化
 ↓
Mask / Weight
```

---

# 13. Pixel Shader 的邻域思维

图像处理 Shader 中，一个 Invocation 通常对应当前 Pixel。

如果想知道周围 Pixel，就需要主动采样：

```text
(x - 1, y)
(x + 1, y)
(x, y - 1)
(x, y + 1)
```

形成四邻域：

```text
          Up
           ↑
           |
Left ← Center → Right
           |
           ↓
         Down
```

于是输出可以写成：

\[
Output(x,y)
=
f(
Input(x,y),
Input(x-1,y),
Input(x+1,y),
Input(x,y-1),
Input(x,y+1)
)
\]

这是很多图像算法的基础：

```text
Blur
Sharpen
Edge Detection
Outline
Bloom
部分 DOF
部分 SSAO
```

---

# 14. Outline 的 Shader 思维

假设：

```text
Stencil = 2
```

表示 Enemy。

当前 Pixel：

```text
Stencil = 0
```

但它右侧 Pixel：

```text
Stencil = 2
```

那么：

```text
当前背景 Pixel | Enemy Pixel
```

正好处在区域边界。

可以写：

```hlsl
float Center =
    IsTarget(CenterStencil);

float Neighbor =
    max(
        max(Left, Right),
        max(Up, Down)
    );

float Outline =
    (1.0f - Center) *
    Neighbor;
```

数学形式：

\[
Outline=(1-C)N
\]

其中：

- \(C\)：当前 Pixel 是否属于目标
- \(N\)：邻居是否存在目标

含义：

> 当前 Pixel 不是目标，但附近存在目标，因此当前 Pixel 属于外描边。

---

# 15. 把 Shader 看成“构造 Mask 的流水线”

很多 Shader 都可以抽象成：

```text
原始数据
 ↓
数据变换
 ↓
归一化
 ↓
提取特征
 ↓
构造 Mask
 ↓
调整 Mask
 ↓
组合 Mask
 ↓
生成最终结果
```

---

## 15.1 Dissolve

```text
Noise Texture
     ↓
Noise Value
     ↓
与 Dissolve Threshold 比较
     ↓
Dissolve Mask
     ↓
Opacity Mask
```

---

## 15.2 Fresnel

```text
Normal
+
View Direction
     ↓
dot(N, V)
     ↓
1 - dot(N, V)
     ↓
pow(...)
     ↓
Fresnel Mask
     ↓
Lerp
```

---

## 15.3 Outline

```text
Stencil
 ↓
邻域采样
 ↓
判断 Target ID
 ↓
Edge Mask
 ↓
Visible Mask
 ↓
Outline Mask
 ↓
Outline Color
```

---

# 16. Shader 不等于“不能使用 if”

错误理解：

> Shader 中不应该使用 `if`。

这是不正确的。

现代 GPU 完全支持：

```hlsl
if (...)
{
}
else
{
}
```

真正需要注意的是：

> 同一个 Warp / Wave 中，不同 Invocation 是否走不同分支。

例如：

```text
Thread 0 → A
Thread 1 → B
Thread 2 → A
Thread 3 → B
```

可能产生：

```text
Branch Divergence
```

因此可以采用下面的原则。

简单条件：

```text
Mask
step
smoothstep
lerp
min
max
```

往往比较合适。

但如果某个分支非常昂贵：

```hlsl
if (NeedRayTracing)
{
    TraceRay();
}
```

就没必要为了“branchless”而强行把两个路径都算一遍。

原则：

> 不要为了消灭一个 `if`，反而执行大量原本不需要的计算。

---

# 17. Shader 常用数学函数的“图形学语义”

不要只记 API，要培养看到函数就知道它大概在解决什么问题的直觉。

| 函数 | 常见图形学意义 |
|---|---|
| `saturate(x)` | 限制到 0～1 |
| `lerp(a,b,t)` | 根据 Mask 混合 |
| `step(a,x)` | 硬阈值 |
| `smoothstep(a,b,x)` | 平滑阈值 |
| `min/max` | 限制、Mask 组合 |
| `abs(x)` | 差异、距离、对称 |
| `dot(a,b)` | 两个方向的夹角关系 |
| `cross(a,b)` | 构造垂直方向 |
| `normalize(v)` | 只保留方向 |
| `length(v)` | 向量长度 |
| `distance(a,b)` | 空间距离 |
| `reflect()` | 镜面反射方向 |
| `refract()` | 折射方向 |
| `pow()` | 调整曲线形状 |
| `exp()` | 指数衰减 |
| `frac()` | 周期、重复 |
| `floor()` | 离散化 |
| `ddx/ddy` | 屏幕空间变化率 |

---

# 18. Shader Debug：把数据画出来

普通 C++ 常用：

```text
断点
printf
Watch
Log
```

Shader Debug 很多时候不方便直接这么做。

最实用的方法是：

> 把中间变量直接输出成颜色。

---

## 18.1 显示 Mask

```hlsl
return float3(
    Mask,
    Mask,
    Mask
);
```

视觉效果：

```text
黑色
→ 0

白色
→ 1

灰色
→ 0～1 中间值
```

---

## 18.2 显示 Normal

法线通常位于：

\[
[-1,1]
\]

显示前映射到：

\[
[0,1]
\]

代码：

```hlsl
return N * 0.5f + 0.5f;
```

---

## 18.3 显示 Depth

```hlsl
return Depth.xxx;
```

实际项目中通常还需要先把深度映射到合适范围，否则可能全部接近黑色或白色。

---

## 18.4 显示 Stencil

例如：

```hlsl
float V =
    Stencil / 255.0f;

return V.xxx;
```

因此一个很重要的 Shader 开发习惯是：

> 每写一步，就把这一阶段的结果可视化。

---

# 19. 不要一次写完整效果

例如做 Outline，不要第一版就同时加入：

```text
Stencil
Depth
Occlusion
Thickness
Glow
Color
Animation
```

应该逐步实现。

```text
Step 1
显示 CustomStencil

Step 2
得到 TargetID Mask

Step 3
读取 Neighbor

Step 4
得到 Edge Mask

Step 5
显示 CustomDepth

Step 6
加入 Visibility 判断

Step 7
加入 Outline Color

Step 8
和 SceneColor 合成
```

这样出现问题时，可以很快定位：

> 到底是哪一步错了。

---

# 20. 阅读 Shader 时不要逐行翻译

看到陌生 Shader，优先找：

```text
Input 是什么？
Output 是什么？
```

然后找：

```text
核心 Mask 是什么？
```

再看：

```text
核心方向是什么？
```

最后看：

```text
最终如何组合？
```

例如：

```hlsl
float NdotV =
    saturate(dot(N, V));

float Fresnel =
    pow(
        1.0f - NdotV,
        5.0f
    );

float3 Color =
    lerp(
        BaseColor,
        EdgeColor,
        Fresnel
    );
```

不要只按代码逐句翻译。

应该直接理解成：

```text
Normal + View Direction
       ↓
计算观察角度
       ↓
构造 Fresnel Mask
       ↓
正面偏 BaseColor
       ↓
边缘偏 EdgeColor
```

---

# 21. Shader 的三类常见数据来源

## 21.1 当前 Invocation 自带数据

例如：

```text
UV
Normal
Position
Vertex Color
Thread ID
```

## 21.2 GPU 全局数据

例如：

```text
Camera
Light
Material
Object Transform
StructuredBuffer
ConstantBuffer
```

## 21.3 空间或邻域查询

例如：

```text
SceneTexture
GBuffer
Neighbor Pixel
Shadow Map
Ray Query
Acceleration Structure
```

所以当发现：

> 当前 Invocation 不知道我要的信息。

不要立刻认为做不到。

应该问：

> 这份信息能不能提前存进某个 GPU Resource，然后让 Shader 查询？

---

# 22. Fragment Shader 和 Compute Shader 的思维并不完全一样

Fragment Shader 很容易形成：

```text
当前 Pixel
+
周围 Pixel
```

这种图像处理思维。

但 Compute Shader 更像普通并行计算：

```hlsl
uint ID =
    DispatchThreadID.x;

ObjectData Object =
    Objects[ID];

Cull(Object);

Results[ID] = ...;
```

对应：

```text
Thread 0 → Object 0
Thread 1 → Object 1
Thread 2 → Object 2
...
```

所以更通用的 Shader 心智模型是：

> 一个 Invocation 负责一个局部 Work Item。

这个 Work Item 可能是：

```text
Pixel
Vertex
Object
Particle
Light
Meshlet
Ray
Tile
Voxel
```

---

# 23. 从 CPU 思维转换到 Shader 思维

CPU 思维：

```text
找到对象
 ↓
判断状态
 ↓
进入不同流程
 ↓
执行操作
```

Shader 思维：

```text
读取当前数据
 ↓
提取特征
 ↓
计算连续数值
 ↓
构造 Mask
 ↓
组合 Mask
 ↓
混合 / 累加
 ↓
输出结果
```

例如 CPU：

```cpp
if (
    IsEnemy &&
    IsVisible &&
    IsEdge
)
{
    Color = Red;
}
```

Shader：

```hlsl
float Mask =
    EnemyMask *
    VisibleMask *
    EdgeMask;

float3 Color =
    lerp(
        SceneColor,
        Red,
        Mask
    );
```

这是非常典型的思维转换。

---

# 24. 一套固定的 Shader 解题流程

以后不知道怎么写 Shader 时，可以直接套下面这套步骤：

```text
1. 确定当前 Invocation 是谁

2. 列出当前已有输入

3. 明确最终输出

4. 找出缺失数据

5. 找到获取缺失数据的方法

6. 把原始数据转换到容易处理的范围

7. 提取需要的特征

8. 构造一个或多个 Mask

9. 组合 Mask

10. 使用 Mask 混合 / 累加最终结果

11. 可视化中间结果

12. 确认正确之后再做性能优化
```

第一版 Shader 不要首先纠结：

```text
能不能少一次乘法？
能不能完全没有 if？
能不能少一个临时变量？
```

正确顺序应该是：

```text
先写对
 ↓
再理解 workload
 ↓
最后优化
```

---

# 25. 推荐的 Shader 思维训练顺序

## 01. UV Gradient

练习：

```text
UV
坐标
颜色映射
```

---

## 02. Component Mask

练习：

```text
Vector
Channel
Swizzle
```

---

## 03. Lerp Color

练习：

```text
Mask
连续混合
```

---

## 04. Step / SmoothStep

练习：

```text
阈值
硬边
软边
```

---

## 05. Fresnel

练习：

```text
dot
pow
方向关系
```

---

## 06. Dissolve

练习：

```text
Noise
Threshold
Mask
```

---

## 07. SceneDepth Visualization

练习：

```text
SceneTexture
Depth
屏幕空间数据
```

---

## 08. CustomStencil Mask

练习：

```text
ID
分类
Mask
```

---

## 09. Neighbor Sampling

练习：

```text
邻域
Pixel Offset
SceneTextureFetch
```

---

## 10. Outline

练习：

```text
Edge Detection
Stencil
Mask Combination
```

---

## 11. 3×3 Blur

练习：

```text
Kernel
Neighbor Sampling
Convolution
```

---

## 12. Sobel Edge Detection

练习：

```text
Gradient
Kernel
Edge
```

---

## 13. Bloom

练习：

```text
HDR
Threshold
Downsample
Blur
Composite
```

---

## 14. SSAO

练习：

```text
Depth
Normal
Random Sampling
Occlusion
```

---

## 15. Ray Query Shadow

练习：

```text
World Position
Ray Direction
Acceleration Structure
Visibility
```

做到这里以后，会发现很多复杂 Shader 都只是这些基础思维的组合。

---

# 26. 常用 Shader 思维公式

## NOT

\[
\neg A
\Rightarrow
1-A
\]

---

## AND

\[
A\land B
\Rightarrow
AB
\]

---

## OR

\[
A\lor B
\Rightarrow
\max(A,B)
\]

---

## 条件选择

\[
Result
=
(1-M)A+MB
\]

对应：

```hlsl
lerp(A, B, M);
```

---

## 硬阈值

\[
M=step(T,x)
\]

---

## 平滑阈值

\[
M=smoothstep(A,B,x)
\]

---

## 方向关系

\[
D=
\mathbf A\cdot\mathbf B
\]

---

## 简单距离 Mask

\[
M=
saturate
\left(
1-\frac{d}{R}
\right)
\]

---

## 外描边

\[
Outline=
(1-C)N
\]

其中：

```text
C = Center Mask
N = Neighbor Mask
```

---

# 27. 最终心智模型

以后写 Shader，可以始终记住下面这条数据流：

```text
            GPU Resources
          /       |       \
     Texture    Buffer   Scene Data
          \       |       /
              ↓
       Current Invocation
              ↓
          Raw Inputs
              ↓
    Mathematical Transform
              ↓
     Feature / Direction
              ↓
             Mask
              ↓
       Combine Masks
              ↓
      Blend / Accumulate
              ↓
            Output
```

Shader 编程的核心并不是：

> 用数学替代正常代码。

真正的核心是：

> 在大规模并行环境下，让每个 Invocation 根据自己能够访问的数据，独立计算对应的输出。

数学、Mask、Texture Sampling、Buffer Query 等技术，只是非常适合这种并行模型的表达方式。

---

# 28. 最值得培养的三个习惯

## 习惯一：先想数据，不要先想代码

不要首先问：

```text
我要写什么 if？
```

先问：

```text
我需要哪些数据？
这些数据在哪里？
```

---

## 习惯二：先构造 Mask

遇到：

```text
哪些 Pixel 应该受到影响？
```

优先思考：

\[
Mask
\]

再用：

```hlsl
lerp(...)
```

或者：

```hlsl
Color *= Mask;
```

处理结果。

---

## 习惯三：不断可视化中间值

如果效果不正确：

```text
不要盯着最终画面猜。
```

应该依次显示：

```text
Input
 ↓
Feature
 ↓
Mask A
 ↓
Mask B
 ↓
Combined Mask
 ↓
Final Color
```

把复杂 Shader 拆成多个可以观察的小问题。

---

# 29. 一句话总结

写普通 C++ 时，习惯问：

> 程序下一步应该做什么？

写 Shader 时，逐渐改成问：

> 当前这个 Invocation 有什么数据，我怎样把这些数据转换成我要的输出？

然后把问题拆成：

```text
Data
 ↓
Feature
 ↓
Mask
 ↓
Combine
 ↓
Output
```

当这种思维形成习惯以后，Shader 就不会再像一种奇怪的编程方式。

它会更像：

> 并行数据处理 + 数学建模 + GPU 资源查询。