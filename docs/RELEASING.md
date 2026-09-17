# Releasing AthenaEnv

AthenaEnv uses two kinds of CI-published builds:

* **Development build** — created automatically on every push to `main`. It overwrites the
  `latest` pre-release, so it always reflects the tip of `main` and is not meant to be
  depended on for stable projects.
* **Tagged release** — created when a `v*` tag (e.g. `v4.0.1`) is pushed. This produces a
  permanent GitHub Release tied to that tag, so games can pin to a known-working version and
  roll back if a newer build causes issues.

## Cutting a tagged release

1. Bump the version in the root [`VERSION`](../VERSION) file (semantic versioning: `MAJOR.MINOR.PATCH`).
2. Commit the bump, e.g. `git commit -am "Bump version to 4.0.1"`.
3. Tag the commit and push the tag:
   ```
   git tag v4.0.1
   git push origin v4.0.1
   ```
4. CI builds the project and publishes a GitHub Release for that tag with `AthenaEnv.tar.gz`
   attached.

The `VERSION` file value and the tag should match so the version embedded in the runtime
(`CONFIG_VERSION`) agrees with the release name.
