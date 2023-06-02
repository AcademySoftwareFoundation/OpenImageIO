# Security Policy

## Supported Versions

This gives guidance about which branches are supported with patches to
security vulnerabilities.

| Version / branch  | Supported                                            |
| ----------------- | ---------------------------------------------------- |
| master            | :white_check_mark: :construction: ALL fixes immediately, but this is a branch under development with a frequently unstable ABI and occasionally unstable API. |
| 2.4.x             | :white_check_mark: All fixes that can be backported without breaking ABI compatibility. New tagged releases monthly. |
| 2.3.x             | :x: Only receives occasional critical fixes, upon request. |
| <= 2.2.x          | :x: No longer receiving patches of any kind.        |


## Reporting a Vulnerability

If you think you've found a potential vulnerability in OpenImageIO, please
report it by emailing security@openimageio.org. Only the project administrators
have access to these messages. Include detailed steps to reproduce the issue,
and any other information that could aid an investigation. Our policy is to
respond to vulnerability reports within 14 days.

Our policy is to address critical security vulnerabilities rapidly and post
patches as quickly as possible.
