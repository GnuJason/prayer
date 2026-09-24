# openSUSE Release Preparation

These are templates, not submission-ready packages. No OBS actions have been
taken. Keep each tool a separate package in `utilities`; submit `prayer` to
Factory first, then `hijri`.

## Required Release Steps

1. The suite is licensed as GPL-3.0-or-later; `LICENSE` contains the complete
   GNU GPL v3 text and the spec templates declare that SPDX expression.
2. Replace `@PROJECT_URL@` in each `.spec.in`, save the resolved files as `.spec`,
   and confirm the published release URL actually exists.
3. Review the libmuslim header licenses and ICU/CLDR table provenance. Ensure
   the packaged libmuslim-devel API matches the pinned headers. Its availability
   in the target OBS project has not been verified.
4. Run `osc vc` in each OBS package checkout to create/maintain its `.changes`
   file using the real maintainer identity. No invented changelog entry is
   supplied here.

## Build and Submission

`make dist` creates separate prayer-1.0.0 and hijri-1.0.0 archives. Each includes
the suite source tree and shared helpers, so either package builds independently
with `TOOLS=prayer` or `TOOLS=hijri`. Each installs only its own executable and
man page. Source duplication keeps the packages independent; binaries do not
depend on one another.

The templates use `%make_build`, `%make_install`, and an offline `%check`.
Distro optimization/linker flags are supplied without dropping warnings or
hardening. Both require libmuslim-devel; prayer also builds with system cJSON
and requires geome as requested. The CLI itself only needs geome for `--auto`.
Installed timezone data is needed for prayer and timezone-related tests.

Build each resolved spec in a clean OBS/openSUSE environment, run packaging
checks, inspect file ownership and notices, and maintain changes with `osc vc`.
Then submit `utilities/prayer` to Factory, followed by `utilities/hijri`.
RPM build tools were not available on the development host; neither RPM builds
nor Factory acceptance have been verified. Fully static libc/libm builds also
need validation on a host with the appropriate static development archives.