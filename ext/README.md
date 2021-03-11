This directory contains external dependencies required by the Skylink protocol implementation.
Only the relevant parts of the dependencies have been copied to this directory appreciating their license agreements
and sharing the gospel of free open source software.
The dependency management is done this way instead of linking to compiled libraries to ease the
development work especially for embedded systems were no established building or dependency management tools exits.

The external dependencies are:

- [**cifra**](https://github.com/ctz/cifra/) licensed under Creative Commons Zero v1.0
for SHA-256 HMAC implementation.

- [**gr-satellites**](https://github.com/daniestevez/gr-satellites) by Daniel Estévez licensed under GPLv3
for Golay24 implementation.

- [**libfec**](http://www.ka9q.net/code/fec/) by Phil Karn under LGPL license
for Reed-Solomon implementation.
