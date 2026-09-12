# Code signing policy

Free code signing provided by [SignPath.io](https://about.signpath.io/), certificate by [SignPath Foundation](https://signpath.org/).

## Scope

Only release artifacts built from this repository may be submitted for signing. The signed Windows executable is built from the public source and build scripts in this repository. Each signing request requires manual approval.

## Team roles

VTSFloat_Meow is maintained by one individual:

- Committer and reviewer: [fushoufish](https://github.com/fushoufish)
- Signing approver: [fushoufish](https://github.com/fushoufish)

Contributions from other people must be reviewed before they are merged. Changes to build scripts, release workflows, dependencies, and signing configuration receive the same review as application source changes.

## Privacy policy

VTSFloat_Meow processes VTube Studio model frames locally. It does not upload or record those frames and does not access VTube Studio's camera or other connected devices. The optional VTube Studio Plugins API connection uses `localhost` only.

This program will not transfer any information to other networked systems unless specifically requested by the user or the person installing or operating it. Project downloads and links opened by the user are governed by the privacy policies of the corresponding third-party services.

## Security and release process

- Repository access and SignPath access use multi-factor authentication.
- Releases are built by GitHub Actions from committed source code.
- Signing requests are reviewed and approved manually.
- The project signs only its own binaries; bundled upstream open-source components retain their own licenses and signatures.
- If a signing key, account, workflow, or release artifact is suspected of compromise, signing is paused while the incident is investigated.
