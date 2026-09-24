# Dependency Provenance

Normal builds prefer installed libmuslim headers and pkg-config `libcjson`.
The following unmodified upstream files provide the offline fallback:

| Dependency | Pinned source | License |
| --- | --- | --- |
| libmuslim | muslimtify-org/libmuslim commit `0a3442690dfc09b57502e3d7d162e676c0f4a146` | MIT notices in each header |
| cJSON | DaveGamble/cJSON tag `v1.7.19` | MIT, [cjson/LICENSE](cjson/LICENSE) |

libmuslim sources: <https://github.com/muslimtify-org/libmuslim/tree/0a3442690dfc09b57502e3d7d162e676c0f4a146>.
Headers: `prayertimes.h` 0.2.4 and `hijri.h` 0.1.1. The latter includes an
ICU/CLDR-derived Umm al-Qura table documented by upstream as Unicode-licensed
data. This application uses only the tabular conversion and discards the unused
astronomical/table sections at link time. Review upstream data notices when
redistributing the complete headers or source archives.

cJSON sources: <https://github.com/DaveGamble/cJSON/tree/v1.7.19>.

SHA-256 checksums:

```text
829c3e514f84d86b7ef0808a9e4ea50a08eac22d0154ff63d025db4dbb1c0a35  libmuslim/hijri.h
1f5e3b594f74a9421d55494a8fbdd39d9764fa9882771a20260e736b7656d365  libmuslim/prayertimes.h
298581a04a36c0165da4b0aade235c23088cb2faa58651d720ea2f3706ed0b0d  vendor/cjson/cJSON.c
25b0145150d500498e4d209cec69c18c42cf818bffcc54690be3b895a2a16dee  vendor/cjson/cJSON.h
a36dda207c36db5818729c54e7ad4e8b0c6fba847491ba64f372c1a2037b6d5c  vendor/cjson/LICENSE
```

No build target downloads dependencies. When updating a snapshot, preserve
upstream notices, update the pin/checksums, and rerun the complete offline suite.
These licenses do not establish a license for the newly written suite code.