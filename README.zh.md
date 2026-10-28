# dr_libs

> 以 C++23 模块使用 dr_wav、dr_flac 与 dr_mp3——WAV/AIFF、FLAC、MP3 解码与 WAV 写入。

[English](README.md) · **简体中文** · [Upstream](https://github.com/mackron/dr_libs) · [Releases](https://github.com/mcpplibs/dr_libs-m/releases) · [mcpp](https://github.com/mcpp-community/mcpp) · [Index](https://github.com/mcpplibs/mcpp-index)

[![C++23](https://img.shields.io/badge/C%2B%2B-23-blue.svg)](https://en.cppreference.com/w/cpp/23)
[![License](https://img.shields.io/badge/license-Apache--2.0-blue.svg)](LICENSE)
[![dr_libs](https://img.shields.io/badge/dr__libs-MIT--0%20OR%20Unlicense-blue.svg)](upstream/dr_libs/LICENSE)
[![mcpp-community](https://img.shields.io/badge/mcpp--community-mwrap-blue)](https://github.com/mcpp-community)
[![CI](https://github.com/mcpplibs/dr_libs-m/actions/workflows/ci.yml/badge.svg?branch=main)](https://github.com/mcpplibs/dr_libs-m/actions/workflows/ci.yml)

dr_libs 是 David Reid 编写的单文件音频 C 库，没有依赖：dr_wav 读取 WAV、
RF64、Wave64 与 AIFF（PCM、浮点、A-law、mu-law、ADPCM）并写入 WAV；dr_flac
解码 FLAC（原生与 Ogg 封装）；dr_mp3 解码 MP3，带 LAME 标签时无缝（gapless）。
许可为公有领域或 MIT-0，二选一。上游 C API 保持不变：`drwav_init_file`、
`drflac_open_memory`、`drmp3_read_pcm_frames_f32` 都是 `<dr_wav.h>`、
`<dr_flac.h>`、`<dr_mp3.h>` 的上游全局名字。库本身是 `compat.dr_libs`
2026.03.03（dr_wav 0.14.5、dr_flac 0.13.3、dr_mp3 0.7.3，同一个上游提交）。
每个头文件各是一个模块，每个宏都是与头文件对照检查过的常量。miniaudio 内嵌的
改名副本可以与它在同一程序中共存。

## 模块

- `import mcpplibs.mwrap.dr_libs;`：三者全部，并重新导出下面的模块
- `import mcpplibs.mwrap.dr_libs.wav;`：`<dr_wav.h>`（`drwav`、`drwav_init_*`、`drwav_read_pcm_frames_*`、`drwav_init_write*`、元数据、格式转换）
- `import mcpplibs.mwrap.dr_libs.flac;`：`<dr_flac.h>`（`drflac`、`drflac_open_*`、`drflac_read_pcm_frames_*`、元数据块及其迭代器）
- `import mcpplibs.mwrap.dr_libs.mp3;`：`<dr_mp3.h>`（`drmp3`、`drmp3_init_*`、`drmp3_read_pcm_frames_*`、定位表、帧解码器 `drmp3dec`）
- `import mcpplibs.mwrap.dr_libs.macros;`：`DRWAV_SUCCESS`、`DR_WAVE_FORMAT_PCM`、`DRFLAC_METADATA_BLOCK_TYPE_STREAMINFO`、`DRMP3_MAX_SAMPLES_PER_FRAME` 等，位于 `mcpplibs::mwrap::dr_libs::macros`（每个头文件模块都重新导出它）

> 更多模块 API -> [用法与 API](docs/usage.md)

## 快速开始

```sh
mcpp new tone --template mcpplibs.mwrap.dr_libs@2026.03.03
cd tone
mcpp run
```

默认模板用 dr_wav 写出 `tone.wav`，逐样本读回核对并给出描述；
`mcpp run -- song.flac` 描述你自己的 WAV、AIFF、FLAC 或 MP3 文件。

```toml
[dependencies]
"mcpplibs.mwrap.dr_libs" = "2026.03.03"
```

```cpp
import std;
import mcpplibs.mwrap.dr_libs;

int main(int argc, char** argv) {
    if (argc < 2) return 2;
    drflac* flac = drflac_open_file(argv[1], nullptr);
    if (flac == nullptr) return 1;
    std::vector<float> pcm(flac->totalPCMFrameCount * flac->channels);
    drflac_uint64 frames = drflac_read_pcm_frames_f32(flac, flac->totalPCMFrameCount, pcm.data());
    std::println("{} frames, {} ch, {} Hz", frames, flac->channels, flac->sampleRate);
    drflac_close(flac);
}
```

## 宏

`DRWAV_SUCCESS`、`DR_WAVE_FORMAT_IEEE_FLOAT` 等常量保持宏本身的类型（`int`）；
版本字符串是 constexpr 字符数组。头文件中的每个枚举都由 typedef 命名，所以其
枚举值（`drwav_container_riff`、`DRWAV_SEEK_SET` 等）按原样导出。`*_API` 属性、
字符串化辅助宏和 dr_flac 的平台检测宏保留为宏，放在 `<mwrap/dr_libs/macros.hpp>`，
在 import 之前包含。细节以及三处值得了解的上游行为（Windows 以外的宽字符路径、
dr_mp3 的定位表、`drwav_f32_to_s16`）见 [docs/usage.md](docs/usage.md)。

## 示例

- [examples/dr_libs](examples/dr_libs)：基于模块编写的音频文件工具——`info` 描述 WAV、AIFF、FLAC 与 MP3 文件
  （格式、时长、峰值与 RMS 电平）；`decode` 把其中任何一种转换为 16 位或浮点 WAV，16 位来源逐样本无损。
  [tools/run_example.py](tools/run_example.py) 在 CI 中用测试数据运行它。

## 构建与测试

Linux（x86_64 GCC 与 LLVM、arm64 GCC）、macOS 和 Windows 的 CI 构建模块，
然后运行测试和导出审计（[tools/check_exports.py](tools/check_exports.py)）。
审计检查生成的源文件是否最新、每个头文件的每个声明是否由其模块导出、每个宏
是否已映射或列出保留原因。测试把 FLAC 测试数据逐样本解码并与其 MD5 所对应的
信号核对，把 MP3 测试数据解码到无缝长度，在 dr_wav 能写的每种容器中往返 WAV，
并把 miniaudio 与 dr_libs 链接在一起、对同一数据解码出相同样本。另有一个任务
以已安装包的方式生成默认模板并运行。

## 许可

封装部分为 [Apache-2.0](LICENSE)，作者 mcpp-community（[NOTICE](NOTICE)）。
dr_libs 为公有领域（Unlicense）或 MIT No Attribution，二选一
（[upstream/dr_libs/LICENSE](upstream/dr_libs/LICENSE)）。其 README 与 LICENSE
随依赖本包的每个程序一同部署。
