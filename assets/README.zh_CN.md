<p align="right">
  <strong>简体中文</strong> · <a href="README.md">English</a>
</p>

# 资源目录（Assets）

本目录集中存放可复用的资源（字库、图片、音乐等），按资源类型分子目录管理。每个资源放在其类型对应的子目录，并记录放置路径、命名方式、集成方式与来源/许可。二进制资源（字体、图片、音频）不属于纯 markdown 文档，请勿与文档混放。涉及版权/授权的资源需注明来源与许可。

## 字库（fonts）

可复用的字库文件与生成的字库源码放在 `fonts/`。

CoffeeNotes 使用 `fonts/CoffeeNotesSansSC-Regular.otf` 以及生成的
`fonts/coffee_font_14.c`、`coffee_font_16.c`、`coffee_font_20.c`。
重命名子集来自 [Noto Sans CJK SC Regular 2.004](https://github.com/notofonts/noto-cjk/tree/Sans2.004)，
版权 Adobe 2014-2021，采用 [SIL Open Font License](fonts/OFL.txt)。
`coffee_glyphs.txt`/`.h` 与 `coffee_font_manifest.json` 保留字符清单及字体哈希。
使用 `fonttools` 与固定的 `lv_font_conv 1.5.3`，运行
`python3 tools/generate_coffee_fonts.py --source-font <licensed-full-font.otf> --converter <lv_font_conv>`；
默认输入为保留的字体子集。三个尺寸均覆盖 ASCII 与全部固定界面文本，4 bpp、不压缩，
由 main 组件编译链接；渲染测试检查实际字体及已知缺字反例。该固定子集不支持任意豆名或
路由器 SSID，设备只展示 ASCII 配网热点名，手机网页使用浏览器字体。

- 命名要能反映字族、字重、字级与格式。
- 记录来源、许可、字符范围、转换命令与目标放置路径。
- 添加字库前评估 Flash 与内部 RAM 影响；ESP32-C3 无 PSRAM。
- 不提交许可不允许分发的字库。

## 图片（images）

可复用的源图与生成的显示资产放在 `images/`。

CoffeeNotes 社区图片为 `images/coffeenotes/cover.png`、`calendar.png` 和 `timer.png`
（240 × 320 PNG，竖版 3:4）。它们使用本项目实际界面与字体，由
`tests/coffee_preview/preview.c` 中的演示记录完成无窗口渲染，不是实机照片或个人数据。
取得 PNG 前已确认渲染测试成功完成；社区提交前逐一打开了实际最终文件检查。
用途为社区封面/配图及双语根目录 README 预览。没有裁剪或缩放。
界面作品遵循仓库许可证，字体来源及许可见上方字库章节。

| 文件 | 尺寸与格式 | 用途与来源 |
| --- | --- | --- |
| [`images/home.jpg`](images/home.jpg) | 3840 × 2160，JPEG | 嵌入中英文项目 README 的产品主图，突出 AI Passport 产品形象与开放、人人可创作的理念。 |
| [`images/readme-hardware-specs.png`](images/readme-hardware-specs.png) | 2172 × 724，PNG RGBA | 保留为可选技术参考图，不再用于首页主视觉。于 2026-09-17 使用内置图像生成工具为本仓库生成；已根据文档中的硬件能力契约核对图中的六项标签与参数。 |
| [`images/logo-wordmark.png`](images/logo-wordmark.png) | 1648 × 336，PNG RGBA | 从仓库原始 `images/logo.png` 中精确裁切并去除背景的黑色字标；用于中英文项目 README 的浅色主题。 |
| [`images/logo-wordmark-dark.png`](images/logo-wordmark-dark.png) | 1648 × 336，PNG RGBA | 提取字标的白色版本；README 使用 `<picture>` 在 GitHub 深色主题下显示。 |

- 使用描述性命名，并记录尺寸、像素格式、转换步骤与目标路径。
- 优先采用适合 240 × 320 RGB565 显示的格式，并纳入 Flash 与内部 RAM 考量。
- 许可允许时保留可编辑源文件，并记录来源与许可。
- 图片中不得包含设备二维码秘密、凭证或个人数据。

## 音乐与音效（music）

可复用的音乐与音效源码放在 `music/`。

- 记录来源、许可、采样率、位深、声道、转换命令与目标路径。
- 与当前 BSP 音频路径匹配时优先采用 16 kHz、16 位单声道 PCM。
- 嵌入音频前评估 Flash 与内部 RAM 成本；长录音应流式或分块。
- 无再分发许可不提交媒体文件。
