# `pd-saf`: Spatial Audio Framework for PureData

![GitHub release (latest by date)](https://img.shields.io/github/v/release/leomccormack/Spatial_Audio_Framework)  
![PureData Compatible](https://img.shields.io/badge/PureData-0.52--tested-blue)  

## About

SAF-PD is a collection of PureData external objects that bring professional spatial audio processing capabilities to PureData, powered by the Spatial Audio Framework (SAF). These objects provide high-performance implementations of various spatial audio algorithms including Ambisonics processing, binaural rendering, VBAP, and more.

## Features

- **Ambisonics Support**: Encoding, decoding, rotation, and binaural rendering.
- **Binaural Processing**: HRTF interpolation and binaural rendering.
- **VBAP**: Vector-based amplitude panning.
- **Room Simulation**: Reverberation and room modeling.
- **High Performance**: Optimized with SIMD and BLAS/LAPACK support.

## Installation

- TODO: Upload on deken.

## Available Objects

### Core Objects

- `saf.binaural~`: Binaural Ambisonic decoder.
- `saf.encoder~`: Ambisonic encoder.
- `saf.decoder~`: Ambisonic decoder.
- `saf.roomsim~`: Shoebox room Ambisonic encoder.

## License

This library is dual-licensed:

- Core modules: ISC License (permissive)
- Optional modules: GPLv2 License

See [LICENSE.md](LICENSE.md) for complete details.

## Contributing

Contributions are welcome! Please open an issue or pull request on GitHub.

## Authors

- Leo McCormack (lead developer)
- SAF contributors.
- Charles K. Neimog

## Support

For questions or support, please open an issue on the GitHub repository.
