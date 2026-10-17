# qingtu-cpp（轻图）

本地图片 **压缩 / 缩放** 与 **横向拼接**，C++ + ImGui + DirectX11 + stb。

## 构建

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
build/Release/qingtu-cpp.exe
```

## 说明

- JPEG / PNG 导出；**WebP 导出请用 Rust 版 [qingtu](../qingtu)**
- 默认输出：`源目录/compressed/`
- 支持拖拽图片到窗口
