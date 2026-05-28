# How to Contribute to the LuaSTG Retro

> The term "we" in the following text refers to the LuaSTG Retro development team.

## Licensing

LuaSTG Retro is currently licensed under the MIT License. Therefore, by submitting code to Luastg Retro, you agree to the following:
* **Your code is licensed under the MIT License.** You may also explicitly provide other optional licenses if you wish.
* Your submission does not contain code that is incompatible with the MIT License. (For example, your submission must not be based on GPL-licensed code, nor may it include GPL libraries.)

## Identify and report defects

Whether you're a player or a game developer, please report any bugs you find in the engine to us.

We offer several ways to report bugs:

* GitHub Issues: <https://github.com/Hoshiruna/Luastg-Retro/issues>
* Email: <hoshiruna05@gmail.com>

Every bug report helps improve the quality of the engine's code, even if you haven't written a single line of code.

## AI / LLM code

**Code generated entirely by AI is prohibited.** While you may refer to suggestions from large language models (LLMs), all submitted code must be written and understood by you (the contributor) personally to ensure its accuracy and copyright integrity.

## Improving Manuals, API Documentation, and Translations

> Programmers hate writing manuals and API documentation the most; other projects often don’t provide them.

If you find any errors or omissions in the manuals or API documentation, please feel free to let us know.

Of course, if you’re very familiar with Git and GitHub, you’re also welcome to submit a pull request.

## Submitting Code

> What programmers hate most is reading other people’s code

Once you are very familiar with Git and GitHub, and have independently developed and fixed some feature improvements and bugs, you can consider creating a Pull Request.

When creating a Pull Request, keep the following in mind:

* Merge the latest commits from the main branch promptly
* Resolve merge conflicts promptly
* If your changes break compatibility, they may not be merged into the main branch but instead into a separate feature branch
* If there are too many changes, reviewing the code may take a long time

## Coding Conventions

LuaSTG Retro requires that the entire codebase adhere to consistent naming conventions. 

Currently, most of these conventions are enforced using clang-format (for C++); any code submitted to LuaSTG Retro must be processed by this tool. In Visual Studio, you can configure the tool to run automatically when you save. On other platforms, you can use the following command:

- clang-format: `find ./ -iname ‘*.h’ -o -iname ‘*.cpp’ | xargs clang-format -i`

Additionally, we require adherence to the following naming conventions:

- ExampleFunction
- exampleVariable
- _exampleMemberVariable

If in doubt, please refer to the formatting guidelines elsewhere in the project.

## Guidelines

> Even if a feature works correctly, a pull request may still be rejected for various reasons. Please be sure to consult with the team before making any major changes.

- Performance: Changes that result in a performance degradation may be rejected. Whenever possible, ensure that your changes avoid adding operations such as `if` statements to hot paths.
- Warnings: The MSVC build of LuaSTG Retro treats warnings as errors. These warnings must be resolved before the code can be accepted.
- Commit Messages: Including background information in your pull request helps us evaluate the code more easily and quickly. If we cannot understand a modification, it is less likely to be accepted.
- Practicality: If we deem a modification or feature to be impractical or outside the scope of the project, it may not be accepted. LuaSTG Retro is not intended to cover all niche use cases.

---

# 如何为 LuaSTG Retro 引擎出谋献策

> 下文中的“我们”指 LuaSTG Retro 引擎开发团队

## 许可协议

LuaSTG Retro 目前采用 MIT 许可协议。因此，向 LuaSTG Retro 提交代码即表示您同意以下条款：
* **您的代码采用 MIT 许可协议。** 如果您愿意，也可以明确提供其他可选许可协议。
* 您的提交内容不包含与 MIT 许可证不兼容的代码。（例如，您的提交内容不得基于 GPL 许可的代码，也不得包含 GPL 库。）

## 发现并报告缺陷

无论你是玩家，还是游戏开发者，发现引擎的缺陷（bug）时，都向我们报告缺陷。

我们提供了多种报告缺陷的途径：

* GitHub Issues：<https://github.com/Hoshiruna/Luastg-Retro/issues>
* 邮箱：<hoshiruna05@gmail.com>

每一个缺陷报告都会帮助改善引擎代码质量，即使你并未编写一行代码。

## AI / LLM 代码

**严禁提交完全由人工智能生成的代码。** 虽然您可以参考大型语言模型（LLM）的建议，但所有提交的代码必须由您（贡献者）亲自编写并理解，以确保其准确性和版权完整性。

## 改进手册、API文档、翻译

> 程序员最讨厌的是编写手册和API文档、他人的项目不提供手册和API文档

如果你发现手册和API文档有错、漏之处，欢迎向我们反馈。

当然，如果你非常熟悉 Git 和 GitHub 的使用，也可以发起 Pull Request。

## 提交代码

> 程序员最讨厌的是阅读他人的代码

当你非常熟悉 Git 和 GitHub 的使用，且一些功能改进和缺陷已经自行开发、修复完成时，可以考虑发起 Pull Request。

发起 Pull Request 时，需要注意：

* 及时合并主分支最新提交
* 及时处理合并冲突
* 存在破坏兼容性的修改时，可能不会合并到主分支，而是合并到单独的特性分支
* 修改内容过多时，审阅代码可能需要花费很长的时间

## 命名规范

LuaSTG Retro 要求整个代码库遵循一致的命名规范。目前，大多数规范是通过 clang-format（针对 C++）来强制执行的；提交给 LuaSTG Retro 的任何代码都必须经过该工具的处理。在 Visual Studio 中，您可以配置该工具，使其在保存时自动运行。在其他平台上，您可以使用以下命令：

* clang-format：`find ./ -iname ‘*.h’ -o -iname ‘*.cpp’ | xargs clang-format -i`

此外，我们要求遵循以下命名规范：

* ExampleFunction
* exampleVariable
* _exampleMemberVariable

如有疑问，请参阅项目中其他位置的格式指南。

## 指南

> 即使某项功能运行正常，拉取请求仍可能因各种原因被拒绝。在进行任何重大更改前，请务必与团队商议。

* 性能：会导致性能下降的修改可能会被拒绝。请尽可能确保您的修改避免在热点路径中添加诸如 `if` 语句之类的操作。
* 警告：LuaSTG Retro 的 MSVC 构建版本将警告视为错误。必须先解决这些警告，代码才能被接受。
* 提交信息：在拉取请求中包含背景信息有助于我们更轻松、更快速地评估代码。如果我们无法理解某项修改，该修改被接受的可能性较低。
* 实用性：若我们认为某项修改或功能不切实际，或超出项目范围，则可能不予接受。LuaSTG Retro 并非旨在覆盖所有小众用例。