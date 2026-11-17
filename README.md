Nagi(凪)
===================================

Nagi は、[Mozc](https://github.com/google/mozc) をもとにした macOS 用の日本語入力です。引っかかりのない、静かな入力を目指しています。

Mozc に次の機能を足しています。

- **遅延付きライブ変換:** スペースキーを押す前に、入力中の文字を自動で変換します。待ち時間と最小の文字数を設定できます。
- **Zenz による補正:** ライブ変換の結果を、この Mac の中で動くニューラルモデル [Zenz](https://huggingface.co/Miwa-Keita/zenz-v3.2-small-gguf) で補正します。入力した内容は外に送りません。
- **変換状態の表示:** 入力中の文字の近くに、いま表示されている文字が何によるものかを示します(入力中 / Mozc / Zenz… / Zenz)。

使い方、ビルド、今の制限は [docs/nagi/README.md](docs/nagi/README.md) に、設計上の判断は [docs/nagi/decisions.md](docs/nagi/decisions.md) にあります。

Nagi は Google 日本語入力ではありません。Google、google/mozc の公式配布物でもなく、Google によるサポートの対象でもありません。ライブ変換、Zenz の扱い、状態表示の設計は、[Mozkey](https://github.com/koyasi777/mozkey) を参考にしています。Mozc のライセンスと著作権表示は [LICENSE](LICENSE) にあります。

以下は、もとになった Mozc の README です。

---

[Mozc - a Japanese Input Method Editor designed for multi-platform](https://github.com/google/mozc)
===================================

Copyright 2010-2026 Google LLC

Mozc is a Japanese Input Method Editor (IME) designed for multi-platform such as
Android OS, Apple macOS, Chromium OS, GNU/Linux and Microsoft Windows.  This
OpenSource project originates from
[Google Japanese Input](http://www.google.com/intl/ja/ime/).

Mozc is not an officially supported Google product.

Build Status
------------

| Linux | Windows | macOS | Android lib |
|:-----:|:-------:|:-----:|:-----------:|
| [![Linux](https://github.com/google/mozc/actions/workflows/linux.yaml/badge.svg)](https://github.com/google/mozc/actions/workflows/linux.yaml) | [![Windows](https://github.com/google/mozc/actions/workflows/windows.yaml/badge.svg)](https://github.com/google/mozc/actions/workflows/windows.yaml) | [![macOS](https://github.com/google/mozc/actions/workflows/macos.yaml/badge.svg)](https://github.com/google/mozc/actions/workflows/macos.yaml) | [![Android lib](https://github.com/google/mozc/actions/workflows/android.yaml/badge.svg)](https://github.com/google/mozc/actions/workflows/android.yaml) |


What's Mozc?
------------
For historical reasons, the project name *Mozc* has two different meanings:

1. Internal code name of Google Japanese Input that is still commonly used
   inside Google.
2. Project name to release a subset of Google Japanese Input in the form of
   source code under OSS license without any warranty nor user support.

In this repository, *Mozc* means the second definition unless otherwise noted.

Detailed differences between Google Japanese Input and Mozc are described in [About Branding](docs/about_branding.md).

For policies on vocabulary and conversion results, see
[Vocabulary Policy](VOCABULARY_POLICY.md).

Build Instructions
------------------

* [How to build Mozc for Android](docs/build_mozc_for_android.md): for Android library (`libmozc.so`)
* [How to build Mozc for Linux](docs/build_mozc_for_linux.md): for Linux desktop
* [How to build Mozc for macOS](docs/build_mozc_in_osx.md): for macOS build
* [How to build Mozc for Windows](docs/build_mozc_in_windows.md): for Windows

Release Plan
------------

tl;dr. **There is no stable version.**

As described in [About Branding](docs/about_branding.md) page, Google does
not promise any official QA for OSS Mozc project.  Because of this,
Mozc does not have a concept of *Stable Release*.  Instead we change version
number every time when we introduce non-trivial change.  If you are
interested in packaging Mozc source code, or developing your own products
based on Mozc, feel free to pick up any version.  They should be equally
stable (or equally unstable) in terms of no official QA process.

[Release History](docs/release_history.md) page may have additional
information and useful links about recent changes.

License
-------

All Mozc code written by Google is released under
[The BSD 3-Clause License](http://opensource.org/licenses/BSD-3-Clause).
For third party code under [src/third_party](src/third_party) directory,
see each sub directory to find the copyright notice.  Note also that
outside [src/third_party](src/third_party) following directories contain
third party code.

### [src/data/dictionary_oss/](src/data/dictionary_oss)
Mixed.
See [src/data/dictionary_oss/README.txt](src/data/dictionary_oss/README.txt)

### [src/data/test/dictionary/](src/data/test/dictionary)
The same as [src/data/dictionary_oss/](src/data/dictionary_oss).
See [src/data/dictionary_oss/README.txt](src/data/dictionary_oss/README.txt)

### [src/data/test/stress_test/](src/data/test/stress_test)
Public Domain.  See the comment in
[src/data/test/stress_test/sentences.txt](src/data/test/stress_test/sentences.txt)
