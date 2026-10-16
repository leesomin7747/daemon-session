# Third-party notices

Daemon Session uses the following upstream components. The macOS bundle includes this file, full license texts under `Contents/Resources/licenses`, and the installed Qt modules' upstream SPDX bills of materials with copyright notices and source locations.

| Component | Version used here | License / source |
| --- | --- | --- |
| Qt Core, Gui, Widgets, Network, Qml, Quick, Quick Controls, Quick Shapes, WebSockets and deployed plugins | 6.11.2 | LGPL-3.0 / GPL alternatives; https://code.qt.io/ and https://download.qt.io/official_releases/qt/6.11/6.11.2/submodules/ |
| QtKeychain | 0.17.0 | BSD-3-Clause; https://github.com/frankosterfeld/qtkeychain/tree/0.17.0 |
| Qt's bundled third-party components | See bundled module SPDX documents | Individual copyright and license identifiers are recorded in each SPDX document; corresponding license texts are included. |
| Apple system frameworks | Supplied by macOS | Not redistributed in this bundle |

Qt and QtKeychain are dynamically linked. The bundle dependency verifier rejects non-system dependencies outside the app; the libraries remain separate and replaceable. No terms of this project restrict modification, reverse engineering for debugging such modifications, or replacement of LGPL libraries. Re-sign a modified local bundle with your own signature as required by macOS; this project does not provide a Gatekeeper bypass.

The packaging host uses the official Homebrew formulae (https://github.com/Homebrew/homebrew-core/tree/master/Formula/q); installed upstream SPDX and version metadata are preserved. Qt source archives above and the matching Homebrew formula/patches identify the corresponding library sources and build configuration. Before any public binary release, audit the final deployed dependency inventory and supply the corresponding sources/required notices with that release.

The Cloudflare Worker uses Cloudflare's runtime. Wrangler, TypeScript and Miniflare are development tools, not bundled into the desktop app. `cloud/package-lock.json` records their exact versions and dependencies. Runtime source contains only this project's server code and the shared approved copy catalogue.
