# Third-Party Components

Copyright (C) 2026 gitzip contributors

This file is part of gitzip.

gitzip is free software: you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free Software
Foundation, either version 3 of the License, or (at your option) any later
version.

## LZMA SDK

- Component: `LZMA SDK`
- Version: `26.00`
- Source: `https://www.7-zip.org/sdk.html`
- Download URL used: `https://www.7-zip.org/a/lzma2600.7z`
- Vendored path: `third_party/lzma-sdk-26.00/`
- Downloaded on: `2026-04-16`

Notes:

- This SDK bundle is the upstream 7-Zip `LZMA SDK`, vendored into the repository so builds do not depend on any system-installed 7-Zip headers or libraries.
- The SDK's own documentation is included under `third_party/lzma-sdk-26.00/DOC/`.
- The main license statement for the SDK is in `third_party/lzma-sdk-26.00/DOC/lzma-sdk.txt`.

Licensing summary:

- `LZMA SDK` is declared by upstream to be placed in the public domain.
- That statement is specific to the SDK bundle.
- The full `7-Zip` application has a different licensing mix and should not be treated as "all public domain".
