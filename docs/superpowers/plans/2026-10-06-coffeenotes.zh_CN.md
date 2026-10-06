[English](2026-10-06-coffeenotes.md) · **简体中文**

# CoffeeNotes 实现计划

**目标：** 在 AI Passport 上用日历记录咖啡类型，并提供可调整的手冲参考参数和实时计时。

**架构：** 日期、记录、配方、输入状态采用独立于 ESP-IDF/LVGL 的纯 C。一个应用工作任务拥有状态和持久化操作，通过 BSP 锁更新独立设计的 LVGL 界面。网络服务使用队列管理 Wi-Fi、临时 WPA2 热点、HTTP 配网和 SNTP。不需要音频或蓝牙栈。

**技术：** ESP-IDF 5.5.3、ESP32-C3、BSP、LVGL 9.5、NVS、esp_http_server、SNTP、Noto CJK 应用字体子集。

用户已授权在关键需求确认后自主开发。保留 CarCard 工作区，在当前工作区 feature/coffeenotes 分支开发，不提交、推送或烧录。

## 1. 日期、记录、配方与交互

- [x] 新增 main/coffee_model.h/.c 和 tests/test_coffee_model.c，覆盖闰年（2000 与 2100）、跨月跨年、周一起始月历、记录计数、容量、明确删除和不受系统校时影响的单调计时暂停恢复。
- [x] 显式版本化小端数据编码和 CRC；加载时验证日期、咖啡类型、配方及参数范围。
- [x] 最多保留 512 条，不自动丢弃旧记录。保存等待持久化结果才显示成功，失败保留草稿以供重试。
- [x] 冷启动后已存日期只作为草稿，SNTP 或人工确认才有效。支持 2020-2099 年，可修改记录日期和查看每日明细。
- [x] 三种明确标注为起点参考的配方，可调整粉量、水量、温度、目标时长，分段累计水量按比例变化；计时支持暂停、取消及完成后进入手冲记录确认。

运行：`cc -std=c11 -Wall -Wextra -Werror -Imain tests/test_coffee_model.c main/coffee_model.c -o /tmp/test_coffee_model && /tmp/test_coffee_model`。
预期日期、记录、编码、计时断言通过；先运行测试观察缺失实现。

## 2. 网络服务

- [x] 新增 main/coffee_network.h/.c，提供线程安全快照及配网、取消、忘记网络的队列接口。网络和 HTTP 不进入 LVGL 锁或按键回调。
- [x] 临时 WPA2 热点限时五分钟，随机八位密码显示在设备上，只允许一个手机，网页地址 http://192.168.4.1。限制并验证表单长度、错误编码及重复字段，使用会话令牌。
- [x] 新凭据连通后才保存；连接失败保留旧凭据；成功、取消、超时后停止 HTTP/热点；已保存 STA 使用有限重试和退避。
- [x] 时区 CST-8，STA 获得 IP 后启动 SNTP；校时有效性独立于网络连接状态。不打印密码，不写入路由器凭据到代码。
- [x] 纯表单解析测试覆盖 UTF-8 SSID 编码、长度上限、错误百分号编码、NUL 注入及短密码/开放网络。

运行网络解析主机测试，再通过 `./tools/validate.sh --firmware` 使用固定 ESP-IDF 编译。配网、IP/NTP、重连及反复生命周期实机测试等待烧录授权。

## 3. 界面、存储和交付

- [x] 替换 main/main.c 启动和 main/CMakeLists.txt 源文件列表；基线示例保留作为参考，但不进入应用启动/构建。
- [x] 新增 coffee_ui.h/.c：暖纸色咖啡色界面，概览、带记录标记的月历、每日记录、记录确认、配方/参数、计时及网络/日期设置。电量右上角，不可用显示占位；长按确定返回或取消，各页给出按键提示。
- [x] 一个工作任务处理输入、存储和重绘，回调只入队；电量读取在 LVGL 锁外；状态/时间变化时才重绘，空闲降低背光，首次按键只唤醒。
- [x] 从全部界面文本生成带许可的可复现 14/16/20 像素中文字体子集，启用 UTF-8/缺字占位；主机渲染检查实际字体、已知缺字反例、文字边界及重复导航内存。
- [x] tools/validate.sh 加入应用主机测试和无窗口渲染，生成固件、PNG 和调试产物保存在被忽略的 build/。
- [x] 双语 docs/apps/coffeenotes.md/.zh_CN.md、资源来源及应用索引；运行完整验证并校验确切内容寻址固件归档。

运行 `./tools/validate.sh` 和 `python3 tools/archive_firmware.py verify build/firmware/<实际完整镜像哈希>`。
预期主机测试、界面审计、固件布局和归档身份通过；交付从 0x0 刷写的合并固件与对应 ELF/MAP，分别报告 Build/Host tests/Device tests/Unverified，再询问是否刷机，未经确认不烧录。
