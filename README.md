# Nanosaur — Native & Web

**[Play Nanosaur in your browser](https://niccolozuppichini.com/Nanosaur/)**

This repository includes a **WebAssembly/WebGL2 port** of [Pangea Software’s Nanosaur](https://www.pangeasoft.net/nano/), built on [Iliyas Jorio’s modern native port](https://github.com/jorio/Nanosaur). The original game and native modernization are their work; our additions bring the game to the browser while retaining the macOS, Windows, and Linux builds.

Originally released in 1998 and bundled with the original iMac, Nanosaur casts a cybernetic dinosaur from the future who’s sent back in time 20 minutes before a giant asteroid hits the Earth.

The web port adds browser rendering and asset loading, fullscreen play, audio, and browser-persisted preferences and high scores. It requires WebGL2 and supports keyboard and gamepad input, but not touch controls.

## Build and play

- **Browser:** [play online](https://niccolozuppichini.com/Nanosaur/) or follow the [web build and hosting instructions](BUILD.md#webassembly--webgl2).
- **Native:** see [BUILD.md](BUILD.md) or download [Jorio’s upstream releases](https://github.com/jorio/Nanosaur/releases).

![Screenshot](docs/screenshot.png)

## Documentation

- [BUILD](BUILD.md) — How to build Nanosaur on macOS, Windows, Linux, or for a WebGL2 browser.
- [CHANGELOG](CHANGELOG.md) — Nanosaur version history.
- [LICENSE](LICENSE.md) — Licensing information (see also below).
- [Instructions](docs/Nanosaur%20Instructions.pdf) — Original instruction manual.
- [Game Engine Information](docs/Nanosaur%20Game%20Engine%20Docs.md) — An overview of Nanosaur’s game engine, written by Brian Greenstone.

## Credits and origins

**Original game:** Nanosaur™ © 1998 Pangea Software, Inc.

- Programming: Brian Greenstone
- Artwork: Scott Harper
- Music: Mike Beckett, Jens Nilsson
- Cinematics: Chris Ashton

**Modern native port:** [Iliyas Jorio](https://github.com/jorio/Nanosaur), who updated and re-released the game with Pangea Software’s permission and created [Pomme](https://github.com/jorio/Pomme), the Macintosh Toolbox compatibility layer used by both the native and web builds.

**This repository’s additions:** the WebAssembly/WebGL2 port, browser-friendly frame lifecycle, and browser launcher.

Pangea released [Nanosaur’s source code](https://www.pangeasoft.net/nano/nanosource.html) in 1999. Jorio’s port builds on that release; this web port builds on his work. Pangea’s [downloads page](https://www.pangeasoft.net/nano/files.html) links to the modern native port.

## License

See [LICENSE.md](LICENSE.md) for the Creative Commons Attribution-NonCommercial-ShareAlike 4.0 license and additional terms.

## Other Pangea game ports

Other Pangea games ported to modern systems by Iliyas Jorio:

- [Billy Frontier](https://github.com/jorio/BillyFrontier)
- [Bugdom](https://github.com/jorio/Bugdom)
- [Cro-Mag Rally](https://github.com/jorio/CroMagRally)
- [Mighty Mike (Power Pete)](https://github.com/jorio/MightyMike)
- [Nanosaur](https://github.com/jorio/Nanosaur)
- [Nanosaur II](https://github.com/jorio/Nanosaur2)
- [Otto Matic](https://github.com/jorio/OttoMatic)
