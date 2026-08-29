# Audio decoder fixtures

These small test inputs exercise the public decoder boundary without adding
commercial or unverified media to the repository. The tests materialize the
checked-in Base64 text into their scoped temporary directory before loading it
through Henka's normal file APIs.

| File | Source | SHA-256 | License/source terms |
| --- | --- | --- | --- |
| `audio_fixture_ogg.b64` | [`love2d/love` clickmono.ogg](https://raw.githubusercontent.com/love2d/love/main/testing/resources/clickmono.ogg) | `87900F83DE1ECCEF7D79199AB336C77186458CDF1718593E6A7E9518FCDA812A` | Upstream LÖVE license |
| `audio_fixture_mp3.b64` | [`lieff/minimp3` MEANDR90.mp3](https://raw.githubusercontent.com/lieff/minimp3/master/vectors/performance/MEANDR90.mp3) | `F252E5A6F10FFBEF00D80095AA6FFEFA231AD7232B8E37AEEA7CFFD760DE0713` | CC0 1.0 Universal |
| `audio_fixture_flac.b64` | [`xiph/flac` input-VA.flac](https://raw.githubusercontent.com/xiph/flac/master/test/flac-to-flac-metadata-test-files/input-VA.flac) | `8730C5E7672781DBA7EF3105DD7BD222425537CAE3D3C5AB237CFDF918B86483` | Xiph BSD-style terms |

The source repositories and their complete license notices remain the
authority for attribution and redistribution terms.
