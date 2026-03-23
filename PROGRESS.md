# 3D Game Engine 開發進度表 (Term Project)

## I. 核心架構與技術規約
- [x] **OpenGL 4.5 Core Profile**: 全面實作現代渲染管線。
- [x] **GLSL Shader Pipeline**: 自定義 PBR、Tessellation、Geometry 著色器。
- [x] **Memory Management**: 所有的幾何資料均管理於 VBO/VAO/EBO 中。

---

## II. 渲染引擎基礎功能
- [x] **PBR 材質模型**: 實作 Cook-Torrance BRDF (GGX + Smith + Schlick)。
- [x] **線性工作流**: 包含 sRGB 線性化、Reinhard Tone Mapping 與 Gamma 校正。
- [x] **多重紋理系統**: 支援 Diffuse, Specular (Mask), Normal 貼圖。
- [x] **陰影映射**: Shadow Mapping 整合 3x3 PCF 濾波。

---

## III. 進階光照與環境模擬
- [x] **Image-Based Lighting (IBL)**:
    *   實作 Split Sum Approximation 近似算法。
    *   整合 Irradiance Map (Diffuse) 與 Prefilter Map (Specular)。
    *   具備離線烘焙 BRDF LUT 查找表。
- [x] **動態水體系統**: 實作雙層法線捲動 (Normal Scrolling) 與物理反射。
- [x] **光源實體化系統**: 太陽 (Directional) 與 點光源 (Point) 均為 Scene Entity。

---

## IV. 幾何處理與效果
- [x] **Tessellation Shader**: 實作動態幾何細分 (四面體轉球體)。
- [x] **Geometry Shader**: 實作 Billboard 粒子系統與幾何爆炸位移。
- [x] **法線貼圖**: 世界空間 TBN 正交化擾動。

---

## 當前開發狀態總結
*   **技術達成度**: 已完成所有預定功能，包含基礎 PBR 渲染與進階著色器組合效果。
*   **系統架構**: 已建立解離的實體管理與渲染封裝系統，具備良好的擴充性。
*   **視覺表現**: 整合 IBL 與線性工作流，提供具備物理正確性的渲染輸出。
