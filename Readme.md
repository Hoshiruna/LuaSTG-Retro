# LuaSTG Retro  

[![C/C++ CI](https://github.com/Hoshiruna/LuaSTG-Retro/actions/workflows/c-cpp.yml/badge.svg)](https://github.com/Hoshiruna/LuaSTG-Retro/actions/workflows/c-cpp.yml)

![Luastg Retro](artwork/logo.png)

---

## Introduction

Luastg-Retro was developed as a fork of [LuaSTG Sub](https://github.com/Legacy-LuaSTG-Engine/LuaSTG-Sub) to cater to the interests of certain retro fans (it updates based on the main branch of LuaSTG Sub).

## Download

You can download from [Releases Page](). Usually displayed on the right or bottom (mobile GitHub page).  

## Major Migration Content

* Graphic API: Direct3D 9 to Direct3D 11
* Audio API: DirectSound to XAudio2

## Features

* Add the PMD music decoding solution within the license
* Modify the game packaging method (Incomplete)
* Modify image scaling mode (Incomplete)

## Requirements

* Operating System: Windows 7 SP1 with platform update ([KB2670838](https://www.microsoft.com/en-us/download/details.aspx?id=36805)) or above
* Graphics Card: compatible with Direct3D11, Direct3D feature level is D3D_FEATURE_LEVEL_10_0 or above
* Audio Card: compatible with XAudio2

## Build Projects

See [Build Projects](./BUILD.md).

## Contributions

See [Contributing](./CONTRIBUTING.md).

## Major Contributors

- 隔壁的桌子 (developer of vanilla LuaSTG)  
- [9chu](https://github.com/9chu) (developer of LuaSTG Plus)  
- [ESC](https://github.com/ExboCooope) (developer of LuaSTG Ex Plus)  
- [Xiliusha](https://github.com/Xiliusha) (developer of LuaSTG Ex Plus)  
- [璀境石](https://github.com/Demonese) (developer of LuaSTG Sub)  

## Special Thanks

- [Natashi](https://github.com/Natashi) (developer of [Danmakufu ph3sx](https://github.com/Natashi/Touhou-Danmakufu-ph3sx-2))  
- And all contributors to danmakufu ph3sx

## Warning

In the past, LuaSTG Retro’s new features utilized code generated entirely by AI and included libraries incompatible with the MIT License. The team is now focused on removing all AI-generated code and eliminating GPL-licensed libraries to mitigate risks.

However, some new features in this branch still rely on AI for development. If you believe this branch poses an uncertain risk to your project, please do not use it.

---

## 介绍  

LuaSTG Retro 是作为 [LuaSTG Sub](https://github.com/Legacy-LuaSTG-Engine/LuaSTG-Sub) 的分支而进行开发的，旨在满足一些怀旧玩家的需求和喜好（它基于主分支LuaSTG Sub来更新）。

其他分支收录在 [Legacy LuaSTG Engine 组织首页](https://github.com/Legacy-LuaSTG-Engine)。  

## 下载  

你可以从 [Releases 页面]() 下载，一般显示在右侧或者底部（手机版页面）。  

## 功能
* 添加在版权许可内的的 PMD 音乐解码方案
* 修改引擎打包方式 (未完成)
* 修改引擎图像缩放模式 (未完成)

## 主要迁移内容  

* 图形 API：从 Direct3D 9 迁移到 Direct3D 11  
* 音频 API：从 DirectSound 迁移到 XAudio2  

## 配置要求  

* 系统要求：最低为 Windows 7 SP1 且安装平台更新（[KB2670838](https://www.microsoft.com/en-us/download/details.aspx?id=36805)）  
* 显卡需求：支持 Direct3D 11 且 Direct3D 功能级别至少为 D3D_FEATURE_LEVEL_10_0  
* 声卡需求：支持 XAudio2  

## 编译项目  

请阅读[编译项目](./BUILD.md)。

## 贡献

请阅读[贡献](./CONTRIBUTING.md)。

## 主要贡献者  

- 隔壁的桌子（LuaSTG 开发者）  
- [9chu](https://github.com/9chu)（LuaSTG Plus 开发者）  
- [ESC](https://github.com/ExboCooope)（LuaSTG Ex Plus 开发者）  
- [Xiliusha](https://github.com/Xiliusha)（LuaSTG Ex Plus 开发者）  
- [璀境石](https://github.com/Demonese)（LuaSTG Sub 开发者）  

## 特别感谢

- [Natashi](https://github.com/Natashi) ([Danmakufu ph3sx](https://github.com/Natashi/Touhou-Danmakufu-ph3sx-2) 开发者)  
- 还有所有 danmakufu ph3sx 的贡献者

## 警告

LuaSTG Retro 在之前一段时间内的新功能采用过完全由 AI 生成的代码和链接了与 MIT 协议不兼容的库，现在团队正致力于清理掉完全由 AI 生成的代码，并且移除 GPL 协议的库来避免产生风险。

但是该分支的一些新功能仍然使用了AI（即人工智能）进行辅助开发，如果您认为该分支会对你的项目产生不确定的风险，请勿尝试使用。
