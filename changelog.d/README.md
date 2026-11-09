# Changelog fragments

A pull request that changes what people who use Mirante see, through this repository itself, adds one file here instead of editing [CHANGELOG.md](../CHANGELOG.md). Pull requests then never change the same lines, so they do not conflict. Most changes come from the core and the modules: the release writes those from their changelogs. See [decision 0154](https://github.com/rrgmc/mirante-core/blob/master/docs/decisions/0154-changelog-fragments.md) in the core.

Name the file after the branch. It holds one or more headings of [Keep a Changelog](https://keepachangelog.com/en/1.1.0/), each with its entries:

```markdown
### Changed

- The Windows installer asks where to install Mirante.
```

The headings are `### Added`, `### Changed`, `### Deprecated`, `### Removed`, `### Fixed` and `### Security`. An entry is a line that starts with `- `, with any further lines indented under it. Write it for people who use Mirante, not for developers.

CI checks every fragment, with the core's `tools/release-changelog.py`. To check them locally, from this repository's root, with the core checked out beside it:

```sh
python3 ../mirante-core/tools/release-changelog.py --root . check
```

A release runs `python3 ../mirante-core/tools/release-changelog.py --root . release X.Y.Z`, which moves the fragments into a section for the new version of `CHANGELOG.md` and deletes them. It does not change `MIRANTE_VERSION`.
