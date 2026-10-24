# Windows code signing (SignPath)

The release workflow Authenticode-signs the CAN Commander executable and its `setup.exe` through
[SignPath](https://signpath.io), which signs open-source projects for free through the
[SignPath Foundation](https://signpath.org). Every signing step is skipped until the repository variable
`SIGNPATH_ORGANIZATION_ID` exists, so releases keep working unsigned until then.

## One-time setup

1. Apply at [signpath.org](https://signpath.org/apply) for the `embedded-pro/e-foc` project and wait for approval.
2. In SignPath, create a project with slug `e-foc` linked to this repository as a trusted GitHub build system.
3. Add two artifact configurations, using the files in [`artifact-configurations/`](artifact-configurations):
   - slug `can-commander-executable`, from `can-commander-executable.xml`
   - slug `can-commander-installer`, from `can-commander-installer.xml`
4. Keep the signing policies SignPath creates: `test-signing` for dry runs and `release-signing` for releases.
5. In this repository's *Settings → Secrets and variables → Actions*:
   - secret `SIGNPATH_API_TOKEN`: an API token of a SignPath CI user that may submit to both policies
   - variable `SIGNPATH_ORGANIZATION_ID`: the SignPath organization ID
   - optional variables `SIGNPATH_PROJECT_SLUG`, `SIGNPATH_TEST_POLICY_SLUG` and `SIGNPATH_RELEASE_POLICY_SLUG`
     when the slugs differ from the ones above
6. Dispatch *Create Release* with `dry_run: true`. The run signs with the test policy, and the installer check
   confirms that both files carry a signature.

## What the workflow does

1. Builds the tools, then submits `e-foc-can-commander.exe` and copies the signed file back into the build
   tree, so the portable zip and the installer both ship the signed executable.
2. Packages and submits the installer, then deletes the unsigned uploads so only signed files reach the release.
3. Installs the signed installer on the runner and checks both signatures, which must be valid for a release.

The `release-signing` policy can require a person to approve each request in SignPath; the workflow waits
up to ten minutes for it.
