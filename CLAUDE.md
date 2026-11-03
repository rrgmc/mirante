# Mirante

The official app of Mirante, a personal desktop dashboard. This repository holds only a manifest, `mirante.json`, a short `CMakeLists.txt`, presets and workflows. The code, the design documents and the decisions are in `mirante-core`, and the modules in `mirante-modules` and `mirante-modules-work`. Make changes to the app there, and move the pins here.

## Git workflow

Each piece of work happens in its own Git worktree, with its own branch. Never edit files in the main checkout on `master` directly.

- `master` is protected on GitHub. Changes reach it only through a pull request whose required CI checks pass.
- Never merge a pull request on your own: only the user merges, on GitHub, by asking for the merge, or by typing `/mmp`. When the user asks for the merge, use a merge commit (`gh pr merge <number> --merge`).
- Commit as work progresses, one focused commit per finished piece.

## Pins

- The core and each repository of modules are pinned to a release tag `vX.Y.Z`. See decision 0134 in `mirante-core`.
- The core's ref is in four places: `mirante.json`, `.github/workflows/ci.yml`, `release.yml` and `update.yml`. CI fails if they differ.

## Releases

A release is a pushed tag `vX.Y.Z`, after a merged pull request that raises `MIRANTE_VERSION` in `CMakeLists.txt`. Pushing a tag publishes, so only push one when the user asks. The `release` environment holds the Developer ID secrets and admits only tags.

`.github/workflows/update.yml` opens a weekly pull request with newer release tags and a raised patch version. It needs the secret `MIRANTE_UPDATE_TOKEN`, a fine-grained token with Contents, Pull requests and Workflows set to Read and write on this repository. After merging one, its tag is pushed by hand.

## README

`README.md` is for people who use Mirante: what it does, the screenshot, the modules, installing, the first start, the author and the licence. Building, releases and the repositories behind Mirante belong in `BUILDING.md`, which the README links to in one line.
