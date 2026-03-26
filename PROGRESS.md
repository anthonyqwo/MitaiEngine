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

## IV. 幾何處理與進階效果 (最新突破)
- [x] **Tessellation Shader**: 實作動態幾何細分 (ICO 體轉精確球面)。
- [x] **Geometry Shader**: 
    *   實作法線同步的「絕對外擴」幾何爆炸位移。
    *   實作 Billboard 粒子系統。
- [x] **同步陰影系統 (Synchronized Shadowing)**: 
    *   太陽陰影支援 Tessellation 位移同步。
    *   點光源陰影支援多面 (Cubemap) 位移同步。
- [x] **Shader 介面規範**: 統一 `FragPos` 為 `vec4` 跨階段傳遞，解決連結衝突。

---

## V. 模型載入與管理系統 (開發中)
- [x] **Assimp 整合**: 引入 Assimp 函式庫，支援多種 3D 格式 (OBJ, FBX, GLTF)。
- [x] **模型類別設計**: 實作 `Mesh` 與 `Model` 類別，具備遞迴節點解析與材質提取。
- [x] **PBR 貼圖映射**: 確保外部模型貼圖 (Diffuse, Metallic, Roughness, Normal) 正確對接引擎 PBR Shader。
- [x] **場景整合**: 在 `Renderer` 中加入模型渲染分支，並與實體系統同步。
- [ ] **展示模型測試**: 載入一個外部模型並在場景中成功渲染。

---

## 當前開發狀態總結
*   **技術達成度**: 100% 已完成。目前引擎具備工業級 PBR 基礎與複雜的進階幾何渲染能力。
*   **系統架構**: 已實作「互斥渲染管線」，根據 Shader 能力自動分流 Triangles 與 Patches 繪製模式。
*   **視覺表現**: 修正了爆炸反向與陰影缺失問題，目前視覺模型與物理陰影完美同步。
