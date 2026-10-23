# Releasing

1. Merge pull requests into `develop`. Each user-visible change brings a changelog fragment.
2. Assemble the changelog on `develop` with a pull request:
   `python3 <icl-common-infra>/scripts/changelog_tool.py assemble --version 1.2.0 --date YYYY-MM-DD`.
   A final release needs the `## [1.2.0]` section this creates, and the Release workflow fails at its
   first job without it. A pre-release (`v1.2.0-rc.1`) can skip this step: its notes are the fragments
   still in `changelog.d/`, which stay there for the final release. `changelog_tool.py notes --version 1.2.0
   --allow-pending` prints the notes a release would get.
3. Tag the merge commit: `git tag -s v1.2.0 && git push origin v1.2.0`. The tag must point at a
   commit on `develop` and must be semantic version form; a suffix such as `-rc.1` makes a pre-release.
   The version before the suffix must equal `VERSION` in `project()` in `CMakeLists.txt` and `version`
   in `vcpkg.json`; the Release workflow stops at its first job when it does not, so change them in the
   pull request that assembles the changelog.
4. The Release workflow builds the Linux, macOS and Windows packages. Windows binaries are handled
   according to the `SIGN_WINDOWS` repository variable: `false`, `test` or `production`.
5. The publish job waits for approval in the `release` environment, then writes `SHA512SUMS`,
   detached GPG signatures, an SPDX SBOM and build provenance, creates the GitHub Release, and for a
   final release fast-forwards `main` to the tag.

`main` only ever moves by that fast-forward. The docs site is deployed from the push to `main`.

## Verifying a download

```bash
gpg --verify SHA512SUMS.asc SHA512SUMS
sha512sum --check SHA512SUMS
gh attestation verify <file> --repo iainchesworthlabs/<repository>
```

The public key is `docs/signing-key.asc`. Its fingerprint is published in `SECURITY.md` of the organisation `.github` repository.

## Windows test signing

Test-signed files carry `-testsigned` in the name and are accepted only on machines that trust the
personal CA. They attach to pre-releases and workflow artifacts only. See
`docs/test-signing.md` in `icl-common-infra`.
