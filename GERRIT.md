# Gerrit submission

Qt does not accept GitHub pull requests. The fork commits already contain
`Change-Id` trailers and are ready for Gerrit.

## Branches

- dev: `a11y-modelchange-reentrancy`
  - commit `cdd276607d2f2099cd9ef8c4d76e35a9059b4f44`
  - Change-Id `I876f86c73a470caf193c1ef6879861cc8b2d305a`
- 6.12: `a11y-modelchange-reentrancy-6.12`
  - commit `762f01ede507d7d9eaaa7419f6c22e45c8a006f0`
  - Change-Id `I1d0dafbe6e7201becacc9fbae9d9d15c72772711`

## Push to Gerrit

Configure a Qt Gerrit account and either an SSH key or an HTTPS HTTP password:

```sh
git clone https://github.com/DrDanielGrace/qtbase.git
cd qtbase
git checkout a11y-modelchange-reentrancy
git push ssh://<user>@codereview.qt-project.org:29418/qt/qtbase     HEAD:refs/for/dev
```

For the 6.12 change:

```sh
git checkout a11y-modelchange-reentrancy-6.12
git push ssh://<user>@codereview.qt-project.org:29418/qt/qtbase     HEAD:refs/for/6.12
```

The local branch was created against upstream `dev` / `6.12` tips:

- dev base: `d19100f37ed4ae341f05abeb5d4d2a4bc289318c`
- 6.12 base: `a3423f9f6d050c6447e8db467810ec916fed1f66`

No GitHub PR is possible on `qt/qtbase` because its GitHub mirror has issues
and pull requests disabled; the REST pulls endpoint returns HTTP 404.
